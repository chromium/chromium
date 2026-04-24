// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/page_action/page_action_controller.h"

#include <memory>
#include <optional>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/timer/timer.h"
#include "chrome/browser/ui/toolbar/pinned_toolbar/pinned_toolbar_actions_model.h"
#include "chrome/browser/ui/views/page_action/chip_selector.h"
#include "chrome/browser/ui/views/page_action/page_action_metrics_recorder.h"
#include "chrome/browser/ui/views/page_action/page_action_metrics_recorder_interface.h"
#include "chrome/browser/ui/views/page_action/page_action_model.h"
#include "chrome/browser/ui/views/page_action/page_action_properties_provider.h"
#include "components/tabs/public/tab_interface.h"
#include "ui/actions/action_id.h"
#include "ui/actions/actions.h"
#include "ui/menus/simple_menu_model.h"

namespace page_actions {

ScopedPageActionActivity::ScopedPageActionActivity(
    PageActionController& controller,
    actions::ActionId action_id)
    : controller_(&controller), action_id_(action_id) {
  RegisterWillDestroyControllerCallback();
}

ScopedPageActionActivity::ScopedPageActionActivity(
    ScopedPageActionActivity&& other) noexcept
    : controller_(other.controller_), action_id_(other.action_id_) {
  RegisterWillDestroyControllerCallback();
  other.controller_ = nullptr;
}

ScopedPageActionActivity& ScopedPageActionActivity::operator=(
    ScopedPageActionActivity&& other) noexcept {
  if (controller_) {
    controller_->DecrementActivityCounter(action_id_);
  }

  action_id_ = other.action_id_;
  controller_ = other.controller_;
  RegisterWillDestroyControllerCallback();
  other.controller_ = nullptr;
  return *this;
}

ScopedPageActionActivity::~ScopedPageActionActivity() {
  if (controller_) {
    controller_->DecrementActivityCounter(action_id_);
  }
}

void ScopedPageActionActivity::RegisterWillDestroyControllerCallback() {
  if (controller_) {
    on_will_destroy_controller_subscription_ =
        controller_->RegisterOnWillDestroyCallback(base::BindOnce(
            [](ScopedPageActionActivity* activity,
               PageActionController& controller) {
              activity->controller_ = nullptr;
            },
            base::Unretained(this)));
  }
}

PageActionControllerImpl::PageActionControllerImpl(
    PinnedToolbarActionsModel* pinned_actions_model,
    PageActionModelFactory* page_action_model_factory,
    PageActionMetricsRecorderFactory* page_action_metrics_recorder_factory)
    : page_action_model_factory_(page_action_model_factory),
      page_action_metrics_recorder_factory_(
          page_action_metrics_recorder_factory) {
  if (pinned_actions_model) {
    pinned_actions_observation_.Observe(pinned_actions_model);
  }
}

PageActionControllerImpl::~PageActionControllerImpl() {
  on_will_destroy_callback_list_.Notify(*this);
}

void PageActionControllerImpl::Initialize(
    tabs::TabInterface& tab_interface,
    const std::vector<actions::ActionId>& action_ids,
    const PageActionPropertiesProviderInterface& properties_provider) {
  tab_activated_callback_subscription_ =
      tab_interface.RegisterDidActivate(base::BindRepeating(
          &PageActionControllerImpl::OnTabActivated, base::Unretained(this)));
  tab_deactivated_callback_subscription_ = tab_interface.RegisterWillDeactivate(
      base::BindRepeating(&PageActionControllerImpl::OnTabWillDeactivate,
                          base::Unretained(this)));
  chip_selector_ = CreateChipSelector(
      base::BindRepeating(&PageActionControllerImpl::DoShowSuggestionChip,
                          base::Unretained(this)),
      base::BindRepeating(&PageActionControllerImpl::DoHideSuggestionChip,
                          base::Unretained(this)),
      base::BindRepeating(&PageActionControllerImpl::DoShowAnchoredMessage,
                          base::Unretained(this)),
      base::BindRepeating(&PageActionControllerImpl::DoHideAnchoredMessage,
                          base::Unretained(this)));

  metrics_recorder_ = CreateMetricsRecorder(
      tab_interface,
      base::BindRepeating(
          &PageActionControllerImpl::GetVisibleEphemeralPageActionsCount,
          base::Unretained(this)));

  for (actions::ActionId id : action_ids) {
    const PageActionProperties& properties =
        properties_provider.GetProperties(id);
    Register(id, tab_interface.IsActivated(), properties.is_ephemeral,
             properties.exempt_from_omnibox_suppression);
    default_priorities_[id] = properties.priority;

    metrics_recorder_->Observe(FindPageActionModel(id), properties);
  }
  if (pinned_actions_observation_.GetSource()) {
    PinnedActionsModelChanged();
  }
}

void PageActionControllerImpl::Register(
    actions::ActionId action_id,
    bool is_tab_active,
    bool is_ephemeral,
    bool is_exempt_from_omnibox_suppression) {
  std::unique_ptr<PageActionModelInterface> model =
      CreateModel(action_id, is_ephemeral);
  model->SetTabActive(PageActionPassKey(), is_tab_active);
  model->SetExemptFromOmniboxSuppression(PageActionPassKey(),
                                         is_exempt_from_omnibox_suppression);
  page_actions_.emplace(action_id, std::move(model));
  // Initialize counter to 0
  activity_counters_[action_id] = 0;
}

void PageActionControllerImpl::Show(actions::ActionId action_id) {
  FindPageActionModel(action_id).SetShowRequested(PageActionPassKey(),
                                                  /*requested=*/true);
}

void PageActionControllerImpl::Hide(actions::ActionId action_id) {
  FindPageActionModel(action_id).SetShowRequested(PageActionPassKey(),
                                                  /*requested=*/false);
}

void PageActionControllerImpl::ShowSuggestionChip(actions::ActionId action_id) {
  ShowSuggestionChip(action_id, SuggestionChipConfig());
}

void PageActionControllerImpl::ShowSuggestionChip(
    actions::ActionId action_id,
    const SuggestionChipConfig& config) {
  if (config.priority == PageActionPriorityCategory::kUnknown &&
      default_priorities_.contains(action_id)) {
    // If the config does not specify the priority level, we fall back to the
    // default one.
    SuggestionChipConfig new_config = config;
    new_config.priority = default_priorities_[action_id];
    chip_selector_->RequestChipShow(action_id, new_config);
  } else {
    chip_selector_->RequestChipShow(action_id, config);
  }
}

void PageActionControllerImpl::DoShowSuggestionChip(
    actions::ActionId action_id,
    const SuggestionChipConfig& config) {
  PageActionModelInterface& model = FindPageActionModel(action_id);
  model.SetSuggestionChipConfig(PageActionPassKey(), config);
  model.SetShouldShowSuggestionChip(PageActionPassKey(), /*show=*/true);
}

void PageActionControllerImpl::HideSuggestionChip(actions::ActionId action_id) {
  chip_selector_->RequestChipHide(action_id);
}

void PageActionControllerImpl::DoHideSuggestionChip(
    actions::ActionId action_id) {
  FindPageActionModel(action_id).SetShouldShowSuggestionChip(
      PageActionPassKey(),
      /*show=*/false);
}

void PageActionControllerImpl::ShowAnchoredMessage(
    actions::ActionId action_id,
    const AnchoredMessageConfig& config) {
  if (config.priority == PageActionPriorityCategory::kUnknown &&
      default_priorities_.contains(action_id)) {
    // If the config does not specify the priority level, we fall back to the
    // default one.
    AnchoredMessageConfig new_config = config;
    new_config.priority = default_priorities_[action_id];
    chip_selector_->RequestAnchoredMessageShow(action_id, new_config);
  } else {
    chip_selector_->RequestAnchoredMessageShow(action_id, config);
  }
}

void PageActionControllerImpl::DoShowAnchoredMessage(
    actions::ActionId action_id,
    const AnchoredMessageConfig& config) {
  FindPageActionModel(action_id).SetShouldShowAnchoredMessage(
      PageActionPassKey(),
      /*show=*/true);
  active_anchored_message_ = action_id;
  anchored_message_timeout_.Start(
      FROM_HERE, base::Seconds(12),
      base::BindRepeating(&PageActionControllerImpl::DowngradeAnchoredMessage,
                          base::Unretained(this), action_id));
}

void PageActionControllerImpl::DowngradeAnchoredMessage(
    actions::ActionId action_id) {
  if (active_anchored_message_ != action_id) {
    return;
  }
  ShowSuggestionChip(action_id);
}

void PageActionControllerImpl::HideAnchoredMessage(
    actions::ActionId action_id) {
  chip_selector_->RequestAnchoredMessageHide(action_id);
}

void PageActionControllerImpl::DoHideAnchoredMessage(
    actions::ActionId action_id) {
  if (active_anchored_message_ == action_id) {
    active_anchored_message_ = std::nullopt;
    if (anchored_message_timeout_.IsRunning()) {
      anchored_message_timeout_.Stop();
    }
  }
  FindPageActionModel(action_id).SetShouldShowAnchoredMessage(
      PageActionPassKey(),
      /*show=*/false);
}

void PageActionControllerImpl::PauseAnchoredMessageTimeout(
    actions::ActionId action_id) {
  if (active_anchored_message_ == action_id &&
      anchored_message_timeout_.IsRunning()) {
    anchored_message_timeout_.Stop();
  }
}

void PageActionControllerImpl::ResumeAnchoredMessageTimeout(
    actions::ActionId action_id) {
  if (active_anchored_message_ == action_id) {
    anchored_message_timeout_.Reset();
  }
}

ScopedPageActionActivity PageActionControllerImpl::AddActivity(
    actions::ActionId action_id) {
  auto& counter = activity_counters_[action_id];
  ++counter;
  FindPageActionModel(action_id).SetActionActive(PageActionPassKey(), true);
  return ScopedPageActionActivity(*this, action_id);
}

void PageActionControllerImpl::DecrementActivityCounter(
    actions::ActionId action_id) {
  auto it = activity_counters_.find(action_id);
  CHECK(it != activity_counters_.end());
  --it->second;
  CHECK_GE(it->second, 0);
  if (it->second == 0) {
    FindPageActionModel(action_id).SetActionActive(PageActionPassKey(), false);
  }
}

void PageActionControllerImpl::ActionItemChanged(
    const actions::ActionItem* action_item) {
  auto& model = FindPageActionModel(action_item->GetActionId().value());
  model.SetActionItemProperties(PageActionPassKey(), action_item);
}

void PageActionControllerImpl::OnTabActivated(tabs::TabInterface* tab) {
  SetModelsTabActive(/*is_active=*/true);
  if (anchored_message_timeout_.IsRunning()) {
    anchored_message_timeout_.Reset();
  }
}

void PageActionControllerImpl::OnTabWillDeactivate(tabs::TabInterface* tab) {
  SetModelsTabActive(/*is_active=*/false);
}

void PageActionControllerImpl::SetModelsTabActive(bool is_active) {
  for (auto& [id, model] : page_actions_) {
    model->SetTabActive(PageActionPassKey(), is_active);
  }
}

void PageActionControllerImpl::OverrideText(
    actions::ActionId action_id,
    const std::u16string& override_text) {
  FindPageActionModel(action_id).SetOverrideText(PageActionPassKey(),
                                                 override_text);
}

void PageActionControllerImpl::ClearOverrideText(actions::ActionId action_id) {
  FindPageActionModel(action_id).SetOverrideText(
      PageActionPassKey(), /*override_text=*/std::nullopt);
}

void PageActionControllerImpl::OverrideAccessibleName(
    actions::ActionId action_id,
    const std::u16string& override_accessible_name) {
  FindPageActionModel(action_id).SetOverrideAccessibleName(
      PageActionPassKey(),
      /*override_accessible_name=*/override_accessible_name);
}

void PageActionControllerImpl::ClearOverrideAccessibleName(
    actions::ActionId action_id) {
  FindPageActionModel(action_id).SetOverrideAccessibleName(
      PageActionPassKey(), /*override_accessible_name=*/std::nullopt);
}

void PageActionControllerImpl::OverrideImage(
    actions::ActionId action_id,
    const ui::ImageModel& override_image) {
  OverrideImage(action_id, override_image, PageActionColorSource::kForeground);
}

void PageActionControllerImpl::OverrideImage(
    actions::ActionId action_id,
    const ui::ImageModel& override_image,
    PageActionColorSource color_source) {
  FindPageActionModel(action_id).SetOverrideImage(PageActionPassKey(),
                                                  override_image, color_source);
}

void PageActionControllerImpl::ClearOverrideImage(actions::ActionId action_id) {
  auto& model = FindPageActionModel(action_id);
  model.SetOverrideImage(PageActionPassKey(),
                         /*override_image=*/std::nullopt,
                         model.GetColorSource());
}

void PageActionControllerImpl::OverrideTooltip(
    actions::ActionId action_id,
    const std::u16string& override_tooltip) {
  FindPageActionModel(action_id).SetOverrideTooltip(PageActionPassKey(),
                                                    override_tooltip);
}

void PageActionControllerImpl::ClearOverrideTooltip(
    actions::ActionId action_id) {
  FindPageActionModel(action_id).SetOverrideTooltip(
      PageActionPassKey(), /*override_tooltip=*/std::nullopt);
}

void PageActionControllerImpl::SetAnchoredMessageText(
    actions::ActionId action_id,
    const std::u16string& anchored_message_text) {
  FindPageActionModel(action_id).SetAnchoredMessageText(PageActionPassKey(),
                                                        anchored_message_text);
}

void PageActionControllerImpl::SetAnchoredMessageIcon(
    actions::ActionId action_id,
    const ui::ImageModel& icon) {
  FindPageActionModel(action_id).SetAnchoredMessageIcon(PageActionPassKey(),
                                                        icon);
}

void PageActionControllerImpl::ClearAnchoredMessageIcon(
    actions::ActionId action_id) {
  FindPageActionModel(action_id).SetAnchoredMessageIcon(PageActionPassKey(),
                                                        std::nullopt);
}

void PageActionControllerImpl::SetAnchoredMessageAction(
    actions::ActionId action_id,
    AnchoredMessageActionIconType action_icon_type,
    std::unique_ptr<ui::SimpleMenuModel> model) {
  if (action_icon_type == AnchoredMessageActionIconType::kMenu) {
    CHECK(model);
  } else {
    CHECK(!model);
  }

  FindPageActionModel(action_id).SetAnchoredMessageAction(
      PageActionPassKey(), action_icon_type, std::move(model));
}

void PageActionControllerImpl::AddObserver(
    actions::ActionId action_id,
    base::ScopedObservation<PageActionModelInterface, PageActionModelObserver>&
        observation) {
  observation.Observe(&FindPageActionModel(action_id));
}

base::CallbackListSubscription
PageActionControllerImpl::CreateActionItemSubscription(
    actions::ActionItem* action_item) {
  base::CallbackListSubscription subscription =
      action_item->AddActionChangedCallback(
          base::BindRepeating(&PageActionControllerImpl::ActionItemChanged,
                              weak_factory_.GetWeakPtr(), action_item));
  ActionItemChanged(action_item);
  return subscription;
}

void PageActionControllerImpl::SetShouldHidePageActions(
    bool should_hide_page_actions) {
  for (auto& [id, model] : page_actions_) {
    model->SetIsSuppressedByOmnibox(PageActionPassKey(),
                                    should_hide_page_actions);
  }
}

void PageActionControllerImpl::OnActionsChanged() {
  PinnedActionsModelChanged();
}

void PageActionControllerImpl::PinnedActionsModelChanged() {
  PinnedToolbarActionsModel* pinned_actions_model =
      pinned_actions_observation_.GetSource();
  CHECK(pinned_actions_model);
  for (auto& [id, model] : page_actions_) {
    const bool is_pinned = pinned_actions_model->Contains(id);
    model->SetHasPinnedIcon(PageActionPassKey(), is_pinned);
  }
}

PageActionModelInterface& PageActionControllerImpl::FindPageActionModel(
    actions::ActionId action_id) const {
  auto id_to_model = page_actions_.find(action_id);
  CHECK(id_to_model != page_actions_.end());
  CHECK(id_to_model->second.get());
  return *id_to_model->second.get();
}

std::unique_ptr<PageActionModelInterface> PageActionControllerImpl::CreateModel(
    actions::ActionId action_id,
    bool is_ephemeral) {
  if (page_action_model_factory_ != nullptr) {
    return page_action_model_factory_->Create(action_id, is_ephemeral);
  } else {
    return std::make_unique<PageActionModel>(action_id, is_ephemeral);
  }
}

std::unique_ptr<PageActionMetricsRecorderInterface>
PageActionControllerImpl::CreateMetricsRecorder(
    tabs::TabInterface& tab_interface,
    VisibleEphemeralPageActionsCountCallback
        visible_ephemeral_page_actions_count_callback) {
  if (page_action_metrics_recorder_factory_ != nullptr) {
    return page_action_metrics_recorder_factory_->CreateRecorder(
        tab_interface,
        std::move(visible_ephemeral_page_actions_count_callback));
  } else {
    return std::make_unique<PageActionMetricsRecorder>(
        tab_interface,
        std::move(visible_ephemeral_page_actions_count_callback));
  }
}

void PageActionControllerImpl::RegisterCallbacks(PageActionPassKey,
                                                 actions::ActionId action_id,
                                                 Delegate* delegate) {
  CHECK(delegate);
  delegate->SetIsChipShowingChangedCallback(
      base::BindRepeating(&PageActionControllerImpl::OnIsChipShowingChanged,
                          weak_factory_.GetWeakPtr(), action_id));
  delegate->SetAnchoredMessageCloseCallback(
      base::BindRepeating(&PageActionControllerImpl::HideAnchoredMessage,
                          weak_factory_.GetWeakPtr(), action_id));
  delegate->SetClickCallback(
      base::BindRepeating(&PageActionControllerImpl::RecordClickMetric,
                          weak_factory_.GetWeakPtr(), action_id));
  delegate->SetAnchoredMessagePauseCallback(base::BindRepeating(
      &PageActionControllerImpl::PauseAnchoredMessageTimeout,
      weak_factory_.GetWeakPtr(), action_id));
  delegate->SetAnchoredMessageResumeCallback(base::BindRepeating(
      &PageActionControllerImpl::ResumeAnchoredMessageTimeout,
      weak_factory_.GetWeakPtr(), action_id));
}

void PageActionControllerImpl::RecordClickMetric(
    actions::ActionId action_id,
    PageActionTrigger trigger_source) {
  metrics_recorder_->RecordClick(action_id, trigger_source);
}

int PageActionControllerImpl::GetVisibleEphemeralPageActionsCount() const {
  int visible_ephemeral_page_actions_count = 0;
  for (auto& [id, model] : page_actions_) {
    if (model->GetVisible() && model->IsEphemeral()) {
      ++visible_ephemeral_page_actions_count;
    }
  }
  return visible_ephemeral_page_actions_count;
}

base::CallbackListSubscription
PageActionControllerImpl::RegisterOnWillDestroyCallback(
    base::OnceCallback<void(PageActionController&)> callback) {
  return on_will_destroy_callback_list_.Add(std::move(callback));
}

void PageActionControllerImpl::OnIsChipShowingChanged(
    actions::ActionId action_id,
    bool is_chip_showing) {
  FindPageActionModel(action_id).SetIsChipShowing(PageActionPassKey(),
                                                  is_chip_showing);
}

std::ostream& operator<<(std::ostream& os, const SuggestionChipConfig& config) {
  os << "{ should_animate: " << config.should_animate
     << ", should_announce_chip: " << config.should_announce_chip << " }";
  return os;
}

}  // namespace page_actions
