// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/toolbar/pinned_toolbar/pinned_translate_action_listener.h"

#include "chrome/browser/translate/chrome_translate_client.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/browser_actions.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "components/translate/core/browser/language_state.h"
#include "ui/actions/actions.h"

PinnedTranslateActionListener::PinnedTranslateActionListener(
    tabs::TabInterface* tab)
    : tab_(tab) {
  AddTranslationObserver(tab_->GetContents());

  tab_subscriptions_.push_back(tab_->RegisterDidEnterForeground(
      base::BindRepeating(&PinnedTranslateActionListener::TabForegrounded,
                          weak_factory_.GetWeakPtr())));

  tab_subscriptions_.push_back(
      tab_->RegisterWillDiscardContents(base::BindRepeating(
          &PinnedTranslateActionListener::OnTabWillDiscardContents,
          weak_factory_.GetWeakPtr())));

  if (tab_->IsInForeground()) {
    UpdateTranslateIndicator();
  }
}

PinnedTranslateActionListener::~PinnedTranslateActionListener() {
  RemoveTranslationObserver(tab_->GetContents());
}

void PinnedTranslateActionListener::OnTabWillDiscardContents(
    tabs::TabInterface* tab,
    content::WebContents* old_contents,
    content::WebContents* new_contents) {
  CHECK_EQ(tab->GetContents(), old_contents);
  RemoveTranslationObserver(old_contents);
  AddTranslationObserver(new_contents);
}

void PinnedTranslateActionListener::OnIsPageTranslatedChanged(
    content::WebContents* source) {
  CHECK_EQ(tab_->GetContents(), source);
  if (tab_->IsInForeground()) {
    UpdateTranslateIndicator();
  }
}

void PinnedTranslateActionListener::AddTranslationObserver(
    content::WebContents* web_contents) {
  translate::ContentTranslateDriver* driver =
      ChromeTranslateClient::FromWebContents(web_contents)->translate_driver();
  driver->AddTranslationObserver(this);
}

void PinnedTranslateActionListener::RemoveTranslationObserver(
    content::WebContents* web_contents) {
  auto* driver =
      ChromeTranslateClient::FromWebContents(web_contents)->translate_driver();
  driver->RemoveTranslationObserver(this);
}

void PinnedTranslateActionListener::TabForegrounded(tabs::TabInterface* tab) {
  UpdateTranslateIndicator();
}

actions::ActionItem* PinnedTranslateActionListener::GetTranslateActionItem(
    BrowserWindowInterface* browser) {
  return actions::ActionManager::Get().FindAction(
      kActionShowTranslate, browser->GetActions()->root_action_item());
}

void PinnedTranslateActionListener::UpdateTranslateIndicator() {
  content::WebContents* web_contents = tab_->GetContents();
  CHECK(web_contents);

  const bool is_page_translated =
      ChromeTranslateClient::FromWebContents(web_contents)
          ->GetLanguageState()
          .IsPageTranslated();

  actions::ActionItem* action_item =
      GetTranslateActionItem(tab_->GetBrowserWindowInterface());

  // TODO(crbug.com/374882730): Add enabled state for action item in this class.
  // Currently logic resides in BrowserCommandController.
  if (action_item) {
    action_item->SetProperty(kActionItemUnderlineIndicatorKey,
                             is_page_translated);
  }
}
