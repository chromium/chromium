// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/page_action/page_action_controller.h"

#include <memory>
#include <utility>

#include "chrome/browser/ui/toolbar/pinned_toolbar/pinned_toolbar_actions_model.h"
#include "chrome/browser/ui/views/page_action/page_action_metrics_recorder.h"
#include "chrome/browser/ui/views/page_action/page_action_metrics_recorder_interface.h"
#include "chrome/browser/ui/views/page_action/page_action_model.h"
#include "chrome/browser/ui/views/page_action/page_action_page_metrics_recorder.h"
#include "chrome/browser/ui/views/page_action/page_action_properties_provider.h"
#include "chrome/browser/ui/views/page_action/page_action_view.h"
#include "components/tabs/public/tab_interface.h"
#include "ui/actions/action_id.h"
#include "ui/actions/actions.h"

namespace page_actions {

using PassKey = base::PassKey<PageActionController>;

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

  page_metrics_recorder_ = CreatePageMetricsRecorder(
      tab_interface,
      base::BindRepeating(
          &PageActionControllerImpl::GetVisibleEphemeralPageActionsCount,
          base::Unretained(this)));

  for (actions::ActionId id : action_ids) {
    const PageActionProperties& properties =
        properties_provider.GetProperties(id);
    Register(id, tab_interface.IsActivated(), properties.is_ephemeral,
             properties.exempt_from_omnibox_suppression);

    // It's safe to use base::Unretained here since the recorded is owned by
    // this object.
    std::unique_ptr<PageActionPerActionMetricsRecorderInterface>
        metrics_recorder = CreatePerActionMetricsRecorder(
            tab_interface, properties, FindPageActionModel(id),
            base::BindRepeating(
                &PageActionControllerImpl::GetVisibleEphemeralPageActionsCount,
                base::Unretained(this)));
    metrics_recorders_.emplace(id, std::move(metrics_recorder));

    // `page_metrics_recorder_` will observe all the page action models to have
    // a global state.
    page_metrics_recorder_->Observe(FindPageActionModel(id));
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
  model->SetTabActive(PassKey(), is_tab_active);
  model->SetExemptFromOmniboxSuppression(PassKey(),
                                         is_exempt_from_omnibox_suppression);
  page_actions_.emplace(action_id, std::move(model));
  // Initialize counter to 0
  activity_counters_[action_id] = 0;
}

void PageActionControllerImpl::Show(actions::ActionId action_id) {
  FindPageActionModel(action_id).SetShowRequested(PassKey(),
                                                  /*requested=*/true);
}

void PageActionControllerImpl::Hide(actions::ActionId action_id) {
  FindPageActionModel(action_id).SetShowRequested(PassKey(),
                                                  /*requested=*/false);
}

void PageActionControllerImpl::ShowSuggestionChip(actions::ActionId action_id) {
  ShowSuggestionChip(action_id, SuggestionChipConfig());
}

void PageActionControllerImpl::ShowSuggestionChip(actions::ActionId action_id,
                                                  SuggestionChipConfig config) {
  PageActionModelInterface& model = FindPageActionModel(action_id);
  model.SetSuggestionChipConfig(PassKey(), config);
  model.SetShouldShowSuggestionChip(PassKey(), /*show=*/true);
}

void PageActionControllerImpl::HideSuggestionChip(actions::ActionId action_id) {
  FindPageActionModel(action_id).SetShouldShowSuggestionChip(PassKey(),
                                                             /*show=*/false);
}

ScopedPageActionActivity PageActionControllerImpl::AddActivity(
    actions::ActionId action_id) {
  auto& counter = activity_counters_[action_id];
  ++counter;
  FindPageActionModel(action_id).SetActionActive(PassKey(), true);
  return ScopedPageActionActivity(*this, action_id);
}

void PageActionControllerImpl::DecrementActivityCounter(
    actions::ActionId action_id) {
  auto it = activity_counters_.find(action_id);
  CHECK(it != activity_counters_.end());
  --it->second;
  CHECK_GE(it->second, 0);
  if (it->second == 0) {
    FindPageActionModel(action_id).SetActionActive(PassKey(), false);
  }
}

void PageActionControllerImpl::ActionItemChanged(
    const actions::ActionItem* action_item) {
  auto& model = FindPageActionModel(action_item->GetActionId().value());
  model.SetActionItemProperties(PassKey(), action_item);
}

void PageActionControllerImpl::OnTabActivated(tabs::TabInterface* tab) {
  SetModelsTabActive(/*is_active=*/true);
}

void PageActionControllerImpl::OnTabWillDeactivate(tabs::TabInterface* tab) {
  SetModelsTabActive(/*is_active=*/false);
}

void PageActionControllerImpl::SetModelsTabActive(bool is_active) {
  for (auto& [id, model] : page_actions_) {
    model->SetTabActive(PassKey(), is_active);
  }
}

void PageActionControllerImpl::OverrideText(
    actions::ActionId action_id,
    const std::u16string& override_text) {
  FindPageActionModel(action_id).SetOverrideText(PassKey(), override_text);
}

void PageActionControllerImpl::ClearOverrideText(actions::ActionId action_id) {
  FindPageActionModel(action_id).SetOverrideText(
      PassKey(), /*override_text=*/std::nullopt);
}

void PageActionControllerImpl::OverrideAccessibleName(
    actions::ActionId action_id,
    const std::u16string& override_accessible_name) {
  FindPageActionModel(action_id).SetOverrideAccessibleName(
      PassKey(), /*override_accessible_name=*/override_accessible_name);
}

void PageActionControllerImpl::ClearOverrideAccessibleName(
    actions::ActionId action_id) {
  FindPageActionModel(action_id).SetOverrideAccessibleName(
      PassKey(), /*override_accessible_name=*/std::nullopt);
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
  FindPageActionModel(action_id).SetOverrideImage(PassKey(), override_image,
                                                  color_source);
}

void PageActionControllerImpl::ClearOverrideImage(actions::ActionId action_id) {
  auto& model = FindPageActionModel(action_id);
  model.SetOverrideImage(PassKey(), /*override_image=*/std::nullopt,
                         model.GetColorSource());
}

void PageActionControllerImpl::OverrideTooltip(
    actions::ActionId action_id,
    const std::u16string& override_tooltip) {
  FindPageActionModel(action_id).SetOverrideTooltip(PassKey(),
                                                    override_tooltip);
}

void PageActionControllerImpl::ClearOverrideTooltip(
    actions::ActionId action_id) {
  FindPageActionModel(action_id).SetOverrideTooltip(
      PassKey(), /*override_tooltip=*/std::nullopt);
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
                              base::Unretained(this), action_item));
  ActionItemChanged(action_item);
  return subscription;
}

void PageActionControllerImpl::SetShouldHidePageActions(
    bool should_hide_page_actions) {
  for (auto& [id, model] : page_actions_) {
    model->SetIsSuppressedByOmnibox(PassKey(), should_hide_page_actions);
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
    model->SetHasPinnedIcon(PassKey(), is_pinned);
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
    return std::make_unique<PageActionModel>(is_ephemeral);
  }
}

std::unique_ptr<PageActionPerActionMetricsRecorderInterface>
PageActionControllerImpl::CreatePerActionMetricsRecorder(
    tabs::TabInterface& tab_interface,
    const PageActionProperties& properties,
    PageActionModelInterface& model,
    VisibleEphemeralPageActionsCountCallback
        visible_ephemeral_page_actions_count_callback) {
  if (page_action_metrics_recorder_factory_ != nullptr) {
    return page_action_metrics_recorder_factory_
        ->CreatePerActionMetricsRecorder(
            tab_interface, properties, model,
            std::move(visible_ephemeral_page_actions_count_callback));
  } else {
    return std::make_unique<PageActionPerActionMetricsRecorder>(
        tab_interface, properties, model,
        std::move(visible_ephemeral_page_actions_count_callback));
  }
}

std::unique_ptr<PageActionPageMetricsRecorderInterface>
PageActionControllerImpl::CreatePageMetricsRecorder(
    tabs::TabInterface& tab_interface,
    VisibleEphemeralPageActionsCountCallback
        visible_ephemeral_page_actions_count_callback) {
  if (page_action_metrics_recorder_factory_ != nullptr) {
    return page_action_metrics_recorder_factory_->CreatePageMetricRecorder(
        tab_interface,
        std::move(visible_ephemeral_page_actions_count_callback));
  } else {
    return std::make_unique<PageActionPageMetricsRecorder>(
        tab_interface,
        std::move(visible_ephemeral_page_actions_count_callback));
  }
}

base::RepeatingCallback<void(PageActionTrigger)>
PageActionControllerImpl::GetClickCallback(base::PassKey<PageActionView>,
                                           actions::ActionId action_id) {
  return base::BindRepeating(&PageActionControllerImpl::RecordClickMetric,
                             weak_factory_.GetWeakPtr(), action_id);
}

void PageActionControllerImpl::RecordClickMetric(
    actions::ActionId action_id,
    PageActionTrigger trigger_source) {
  auto id_and_recorder = metrics_recorders_.find(action_id);
  CHECK(id_and_recorder != metrics_recorders_.end());
  CHECK(id_and_recorder->second.get());
  id_and_recorder->second->RecordClick(trigger_source);
}

int PageActionControllerImpl::GetVisibleEphemeralPageActionsCount() const {
  int visible_ephemeral_page_actions_count = 0;
  for (auto& [id, model] : page_actions_) {
    CHECK(metrics_recorders_.contains(id));
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

void PageActionControllerImpl::RegisterIsChipShowingChangedCallback(
    base::PassKey<PageActionView>,
    actions::ActionId action_id,
    PageActionView* page_action_view) {
  CHECK(page_action_view);
  page_action_view->SetIsChipShowingChangedCallback(
      base::BindRepeating(&PageActionControllerImpl::OnIsChipShowingChanged,
                          weak_factory_.GetWeakPtr(), action_id));
}

void PageActionControllerImpl::OnIsChipShowingChanged(
    actions::ActionId action_id,
    bool is_chip_showing) {
  FindPageActionModel(action_id).SetIsChipShowing(PassKey(), is_chip_showing);
}

std::ostream& operator<<(std::ostream& os, const SuggestionChipConfig& config) {
  os << "{ should_animate: " << config.should_animate
     << ", should_announce_chip: " << config.should_announce_chip << " }";
  return os;
}

}  // namespace page_actions
