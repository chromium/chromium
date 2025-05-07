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

PageActionController::PageActionController(
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

PageActionController::~PageActionController() = default;

void PageActionController::Initialize(
    tabs::TabInterface& tab_interface,
    const std::vector<actions::ActionId>& action_ids,
    const PageActionPropertiesProviderInterface& properties_provider) {
  tab_activated_callback_subscription_ =
      tab_interface.RegisterDidActivate(base::BindRepeating(
          &PageActionController::OnTabActivated, base::Unretained(this)));
  tab_deactivated_callback_subscription_ =
      tab_interface.RegisterWillDeactivate(base::BindRepeating(
          &PageActionController::OnTabWillDeactivate, base::Unretained(this)));

  page_metrics_recorder_ = CreatePageMetricsRecorder(
      tab_interface,
      base::BindRepeating(
          &PageActionController::GetVisibleEphemeralPageActionsCount,
          base::Unretained(this)));

  for (actions::ActionId id : action_ids) {
    const PageActionProperties& properties =
        properties_provider.GetProperties(id);
    Register(id, tab_interface.IsActivated(), properties.is_ephemeral);

    // It's safe to use base::Unretained here since the recorded is owned by
    // this object.
    std::unique_ptr<PageActionPerActionMetricsRecorderInterface>
        metrics_recorder = CreatePerActionMetricsRecorder(
            tab_interface, properties, FindPageActionModel(id),
            base::BindRepeating(
                &PageActionController::GetVisibleEphemeralPageActionsCount,
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

void PageActionController::Register(actions::ActionId action_id,
                                    bool is_tab_active,
                                    bool is_ephemeral) {
  std::unique_ptr<PageActionModelInterface> model =
      CreateModel(action_id, is_ephemeral);
  model->SetTabActive(PassKey(), is_tab_active);
  page_actions_.emplace(action_id, std::move(model));
}

void PageActionController::Show(actions::ActionId action_id) {
  FindPageActionModel(action_id).SetShowRequested(PassKey(),
                                                  /*requested=*/true);
}

void PageActionController::Hide(actions::ActionId action_id) {
  FindPageActionModel(action_id).SetShowRequested(PassKey(),
                                                  /*requested=*/false);
}

void PageActionController::ShowSuggestionChip(actions::ActionId action_id,
                                              SuggestionChipConfig config) {
  PageActionModelInterface& model = FindPageActionModel(action_id);
  model.SetSuggestionChipConfig(PassKey(), config);
  model.SetShowSuggestionChip(PassKey(), /*show=*/true);
}

void PageActionController::HideSuggestionChip(actions::ActionId action_id) {
  FindPageActionModel(action_id).SetShowSuggestionChip(PassKey(),
                                                       /*show=*/false);
}

void PageActionController::ActionItemChanged(
    const actions::ActionItem* action_item) {
  auto& model = FindPageActionModel(action_item->GetActionId().value());
  model.SetActionItemProperties(PassKey(), action_item);
}

void PageActionController::OnTabActivated(tabs::TabInterface* tab) {
  SetModelsTabActive(/*is_active=*/true);
}

void PageActionController::OnTabWillDeactivate(tabs::TabInterface* tab) {
  SetModelsTabActive(/*is_active=*/false);
}

void PageActionController::SetModelsTabActive(bool is_active) {
  for (auto& [id, model] : page_actions_) {
    model->SetTabActive(PassKey(), is_active);
  }
}

void PageActionController::OverrideText(actions::ActionId action_id,
                                        const std::u16string& override_text) {
  FindPageActionModel(action_id).SetOverrideText(PassKey(), override_text);
}

void PageActionController::ClearOverrideText(actions::ActionId action_id) {
  FindPageActionModel(action_id).SetOverrideText(
      PassKey(), /*override_text=*/std::nullopt);
}

void PageActionController::OverrideAccessibleName(
    actions::ActionId action_id,
    const std::u16string& override_accessible_name) {
  FindPageActionModel(action_id).SetOverrideAccessibleName(
      PassKey(), /*override_accessible_name=*/override_accessible_name);
}

void PageActionController::ClearOverrideAccessibleName(
    actions::ActionId action_id) {
  FindPageActionModel(action_id).SetOverrideAccessibleName(
      PassKey(), /*override_accessible_name=*/std::nullopt);
}

void PageActionController::OverrideImage(actions::ActionId action_id,
                                         const ui::ImageModel& override_image) {
  FindPageActionModel(action_id).SetOverrideImage(PassKey(), override_image);
}

void PageActionController::ClearOverrideImage(actions::ActionId action_id) {
  FindPageActionModel(action_id).SetOverrideImage(
      PassKey(), /*override_image=*/std::nullopt);
}

void PageActionController::OverrideTooltip(
    actions::ActionId action_id,
    const std::u16string& override_tooltip) {
  FindPageActionModel(action_id).SetOverrideTooltip(PassKey(),
                                                    override_tooltip);
}

void PageActionController::ClearOverrideTooltip(actions::ActionId action_id) {
  FindPageActionModel(action_id).SetOverrideTooltip(
      PassKey(), /*override_tooltip=*/std::nullopt);
}

void PageActionController::AddObserver(
    actions::ActionId action_id,
    base::ScopedObservation<PageActionModelInterface, PageActionModelObserver>&
        observation) {
  observation.Observe(&FindPageActionModel(action_id));
}

base::CallbackListSubscription
PageActionController::CreateActionItemSubscription(
    actions::ActionItem* action_item) {
  base::CallbackListSubscription subscription =
      action_item->AddActionChangedCallback(
          base::BindRepeating(&PageActionController::ActionItemChanged,
                              base::Unretained(this), action_item));
  ActionItemChanged(action_item);
  return subscription;
}

void PageActionController::SetShouldHidePageActions(
    bool should_hide_page_actions) {
  for (auto& [id, model] : page_actions_) {
    model->SetShouldHidePageAction(PassKey(), should_hide_page_actions);
  }
}

void PageActionController::OnActionsChanged() {
  PinnedActionsModelChanged();
}

void PageActionController::PinnedActionsModelChanged() {
  PinnedToolbarActionsModel* pinned_actions_model =
      pinned_actions_observation_.GetSource();
  CHECK(pinned_actions_model);
  for (auto& [id, model] : page_actions_) {
    const bool is_pinned = pinned_actions_model->Contains(id);
    model->SetHasPinnedIcon(PassKey(), is_pinned);
  }
}

PageActionModelInterface& PageActionController::FindPageActionModel(
    actions::ActionId action_id) const {
  auto id_to_model = page_actions_.find(action_id);
  CHECK(id_to_model != page_actions_.end());
  CHECK(id_to_model->second.get());
  return *id_to_model->second.get();
}

std::unique_ptr<PageActionModelInterface> PageActionController::CreateModel(
    actions::ActionId action_id,
    bool is_ephemeral) {
  if (page_action_model_factory_ != nullptr) {
    return page_action_model_factory_->Create(action_id, is_ephemeral);
  } else {
    return std::make_unique<PageActionModel>(is_ephemeral);
  }
}

std::unique_ptr<PageActionPerActionMetricsRecorderInterface>
PageActionController::CreatePerActionMetricsRecorder(
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
PageActionController::CreatePageMetricsRecorder(
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
PageActionController::GetClickCallback(actions::ActionId action_id) {
  return base::BindRepeating(&PageActionController::RecordClickMetric,
                             weak_factory_.GetWeakPtr(), action_id);
}

void PageActionController::RecordClickMetric(actions::ActionId action_id,
                                             PageActionTrigger trigger_source) {
  auto id_and_recorder = metrics_recorders_.find(action_id);
  CHECK(id_and_recorder != metrics_recorders_.end());
  CHECK(id_and_recorder->second.get());
  id_and_recorder->second->RecordClick(trigger_source);
}

int PageActionController::GetVisibleEphemeralPageActionsCount() const {
  int visible_ephemeral_page_actions_count = 0;
  for (auto& [id, model] : page_actions_) {
    CHECK(metrics_recorders_.contains(id));
    if (model->GetVisible() && model->IsEphemeral()) {
      ++visible_ephemeral_page_actions_count;
    }
  }
  return visible_ephemeral_page_actions_count;
}

std::ostream& operator<<(std::ostream& os, const SuggestionChipConfig& config) {
  os << "{ should_animate: " << config.should_animate
     << ", should_announce_chip: " << config.should_announce_chip << " }";
  return os;
}

}  // namespace page_actions
