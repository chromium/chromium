// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/translate_browser_action_listener.h"

#include "chrome/browser/translate/chrome_translate_client.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_actions.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "components/translate/core/browser/language_state.h"
#include "ui/actions/action_id.h"
#include "ui/actions/actions.h"

TranslateBrowserActionListener::TranslateBrowserActionListener(Browser& browser)
    : browser_(browser) {
  browser_->tab_strip_model()->AddObserver(this);
}

TranslateBrowserActionListener::~TranslateBrowserActionListener() {
  TabStripModel* tab_strip_model = browser_->tab_strip_model();

  tab_strip_model->RemoveObserver(this);
  RemoveTranslationObserver(tab_strip_model->GetActiveWebContents());
}

void TranslateBrowserActionListener::OnTabStripModelChanged(
    TabStripModel* tab_strip_model,
    const TabStripModelChange& change,
    const TabStripSelectionChange& selection) {
  if (selection.active_tab_changed()) {
    RemoveTranslationObserver(selection.old_contents);
    AddTranslationObserver(selection.new_contents);

    actions::ActionItem* action_item = actions::ActionManager::Get().FindAction(
        kActionShowTranslate, browser_->browser_actions()->root_action_item());

    if (selection.new_contents) {
      const bool is_page_translated =
          ChromeTranslateClient::FromWebContents(selection.new_contents)
              ->GetLanguageState()
              .IsPageTranslated();
      action_item->SetProperty(kActionItemUnderlineIndicatorKey,
                               is_page_translated);
    } else {
      action_item->ClearProperty(kActionItemUnderlineIndicatorKey);
    }
  }
}

void TranslateBrowserActionListener::AddTranslationObserver(
    content::WebContents* web_contents) {
  if (!web_contents) {
    return;
  }

  translate::ContentTranslateDriver* content_translate_driver =
      ChromeTranslateClient::FromWebContents(web_contents)->translate_driver();
  content_translate_driver->AddTranslationObserver(this);
}

void TranslateBrowserActionListener::RemoveTranslationObserver(
    content::WebContents* web_contents) {
  if (!web_contents) {
    return;
  }

  translate::ContentTranslateDriver* content_translate_driver =
      ChromeTranslateClient::FromWebContents(web_contents)->translate_driver();
  content_translate_driver->RemoveTranslationObserver(this);
}

void TranslateBrowserActionListener::OnIsPageTranslatedChanged(
    content::WebContents* source) {
  DCHECK(browser_->tab_strip_model()->GetActiveWebContents() == source);

  actions::ActionItem* action_item = actions::ActionManager::Get().FindAction(
      kActionShowTranslate, browser_->browser_actions()->root_action_item());

  const bool is_page_translated = ChromeTranslateClient::FromWebContents(source)
                                      ->GetLanguageState()
                                      .IsPageTranslated();
  action_item->SetProperty(kActionItemUnderlineIndicatorKey,
                           is_page_translated);
}
