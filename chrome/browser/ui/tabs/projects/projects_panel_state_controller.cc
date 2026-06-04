// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/projects/projects_panel_state_controller.h"

#include "chrome/browser/glic/public/glic_keyed_service.h"
#include "chrome/browser/ui/actions/actions_util.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/user_education/browser_user_education_interface.h"
#include "chrome/grit/generated_resources.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/omnibox/browser/aim_eligibility_service.h"
#include "ui/actions/actions.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/vector_icons.h"

DEFINE_USER_DATA(ProjectsPanelStateController);

ProjectsPanelStateController::ProjectsPanelStateController(
    BrowserWindowInterface* browser_window,
    actions::ActionItem* root_action_item,
    AimEligibilityService* aim_eligibility_service,
    glic::GlicEnabling* glic_enabling)
    : root_action_item_(root_action_item),
      aim_eligibility_service_(aim_eligibility_service),
      glic_enabling_(glic_enabling),
      scoped_unowned_user_data_(browser_window->GetUnownedUserDataHost(),
                                *this) {
  if (aim_eligibility_service_) {
    can_show_aim_threads_ = aim_eligibility_service_->IsAimEligible();
    aim_eligibility_changed_subcription_ =
        aim_eligibility_service_->RegisterEligibilityChangedCallback(
            base::BindRepeating(
                [](base::WeakPtr<ProjectsPanelStateController> weak_this) {
                  if (!weak_this) {
                    return;
                  }
                  weak_this->OnAimEligibilityChanged();
                },
                weak_ptr_factory_.GetWeakPtr()));
  }

  if (glic_enabling_) {
    can_show_gemini_threads_ = glic_enabling_->IsAllowed();
    gemini_eligibility_changed_subcription_ =
        glic_enabling_->RegisterAllowedChanged(base::BindRepeating(
            [](base::WeakPtr<ProjectsPanelStateController> weak_this) {
              if (!weak_this) {
                return;
              }
              weak_this->OnGeminiEligibilityChanged();
            },
            weak_ptr_factory_.GetWeakPtr()));
  }

  UpdateProjectsActionItem();
}

ProjectsPanelStateController::~ProjectsPanelStateController() = default;

// static
ProjectsPanelStateController* ProjectsPanelStateController::From(
    BrowserWindowInterface* browser_window) {
  return Get(browser_window->GetUnownedUserDataHost());
}

bool ProjectsPanelStateController::IsProjectsPanelVisible() const {
  return is_visible_;
}

void ProjectsPanelStateController::SetProjectsVisible(bool visible) {
  if (is_visible_ == visible) {
    return;
  }

  is_visible_ = visible;
  NotifyStateChanged();
}

bool ProjectsPanelStateController::CanShowAimThreads() {
  return can_show_aim_threads_;
}

bool ProjectsPanelStateController::CanShowGeminiThreads() {
  return can_show_gemini_threads_;
}

base::CallbackListSubscription
ProjectsPanelStateController::RegisterOnStateChanged(
    StateChangedCallback callback) {
  return on_state_changed_callback_list_.Add(std::move(callback));
}

base::CallbackListSubscription
ProjectsPanelStateController::RegisterOnThreadEligibilityChanged(
    ThreadEligibilityChangedCallback callback) {
  return on_thread_eligibility_changed_callback_list_.Add(std::move(callback));
}

void ProjectsPanelStateController::NotifyStateChanged() {
  UpdateProjectsActionItem();
  on_state_changed_callback_list_.Notify(this);
}

void ProjectsPanelStateController::UpdateProjectsActionItem() {
  const auto& text = IsProjectsPanelVisible() ? IDS_HIDE_PROJECTS_PANEL
                                              : IDS_VIEW_PROJECTS_PANEL;

  actions::ActionItem* projects_action =
      actions::ActionManager::Get().FindAction(kActionToggleProjectsPanel,
                                               root_action_item_);
  if (projects_action) {
    projects_action->SetText(
        chrome::GetCleanTitleAndTooltipText(l10n_util::GetStringUTF16(text)));
    projects_action->SetTooltipText(
        chrome::GetCleanTitleAndTooltipText(l10n_util::GetStringUTF16(text)));
  }
}

void ProjectsPanelStateController::NotifyThreadEligibilityChanged() {
  on_thread_eligibility_changed_callback_list_.Notify(this);
}

void ProjectsPanelStateController::OnAimEligibilityChanged() {
  can_show_aim_threads_ = aim_eligibility_service_->IsAimEligible();
  NotifyThreadEligibilityChanged();
}

void ProjectsPanelStateController::OnGeminiEligibilityChanged() {
  can_show_gemini_threads_ = glic_enabling_->IsAllowed();
  NotifyThreadEligibilityChanged();
}
