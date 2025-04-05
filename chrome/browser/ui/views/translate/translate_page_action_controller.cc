// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/translate/translate_page_action_controller.h"

#include "base/check_deref.h"
#include "base/functional/bind.h"
#include "chrome/browser/translate/chrome_translate_client.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/browser_actions.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/page_action/page_action_icon_type.h"
#include "chrome/browser/ui/tabs/public/tab_features.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/page_action/page_action_controller.h"
#include "chrome/browser/ui/views/translate/translate_bubble_controller.h"
#include "components/tabs/public/tab_interface.h"
#include "components/translate/core/browser/language_state.h"
#include "components/translate/core/browser/translate_manager.h"
#include "ui/actions/actions.h"
#include "ui/base/ui_base_features.h"

namespace {
ChromeTranslateClient& GetTranslateClient(
    const tabs::TabInterface& tab_interface) {
  ChromeTranslateClient* client =
      ChromeTranslateClient::FromWebContents(tab_interface.GetContents());
  CHECK(client);
  return *client;
}
}  // namespace

TranslatePageActionController::TranslatePageActionController(
    tabs::TabInterface& tab_interface)
    : PageActionObserver(kActionShowTranslate), tab_interface_(tab_interface) {
  CHECK(IsPageActionMigrated(PageActionIconType::kTranslate));
  translate_observation_.Observe(
      GetTranslateClient(tab_interface).translate_driver());
  will_discard_contents_subscription_ =
      tab_interface_->RegisterWillDiscardContents(base::BindRepeating(
          &TranslatePageActionController::WillDiscardContents,
          base::Unretained(this)));

  RegisterAsPageActionObserver(
      CHECK_DEREF(tab_interface_->GetTabFeatures()->page_action_controller()));

  // Translation may be enabled already at the time of creating the tab (e.g.,
  // while moving contents from a browser windows to a web app).
  UpdatePageAction();
}

TranslatePageActionController::~TranslatePageActionController() = default;

void TranslatePageActionController::OnPageActionIconShown(
    const page_actions::PageActionState& page_action) {
  RecordIconChange(true);
}

void TranslatePageActionController::OnPageActionIconHidden(
    const page_actions::PageActionState& page_action) {
  RecordIconChange(false);
}

void TranslatePageActionController::OnTranslateEnabledChanged(
    content::WebContents* source) {
  UpdatePageAction();
}

void TranslatePageActionController::RecordIconChange(bool is_showing) {
  GetTranslateClient(tab_interface_.get())
      .GetTranslateManager()
      ->GetActiveTranslateMetricsLogger()
      ->LogOmniboxIconChange(is_showing);
}

void TranslatePageActionController::WillDiscardContents(
    tabs::TabInterface* tab_interface,
    content::WebContents* old_contents,
    content::WebContents* new_contents) {
  translate_observation_.Reset();
  translate_observation_.Observe(
      ChromeTranslateClient::FromWebContents(new_contents)->translate_driver());
}

void TranslatePageActionController::UpdatePageAction() {
  if (!tab_interface_->GetContents()) {
    return;
  }

  ChromeTranslateClient& translate_client =
      GetTranslateClient(tab_interface_.get());
  const translate::LanguageState& language_state =
      translate_client.GetLanguageState();
  const bool translate_enabled = language_state.translate_enabled();

  page_actions::PageActionController* page_action_controller =
      tab_interface_->GetTabFeatures()->page_action_controller();
  CHECK(page_action_controller);

  if (translate_enabled) {
    // TODO(crbug.com/404190582): Currently, the translate manager doesn't
    // update the action synchronously, and waits on tab state changes for
    // updates instead, making it possible the action to be out of sync. If
    // translation is enabled, then we should be ok to show the translate  UI,
    // so we manually enable the action here. This should be removed once the
    // bug is fixed.
    actions::ActionManager::Get()
        .FindAction(kActionShowTranslate,
                    tab_interface_->GetBrowserWindowInterface()
                        ->GetActions()
                        ->root_action_item())
        ->SetEnabled(true);
    page_action_controller->Show(kActionShowTranslate);
  } else {
    page_action_controller->Hide(kActionShowTranslate);
    if (TranslateBubbleController* bubble_controller =
            tab_interface_->GetBrowserWindowInterface()
                ->GetFeatures()
                .translate_bubble_controller()) {
      bubble_controller->CloseBubble();
    }
  }
}
