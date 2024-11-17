// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TOOLBAR_PINNED_TOOLBAR_PINNED_TRANSLATE_ACTION_LISTENER_H_
#define CHROME_BROWSER_UI_TOOLBAR_PINNED_TOOLBAR_PINNED_TRANSLATE_ACTION_LISTENER_H_

#include <vector>

#include "base/callback_list.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/tabs/public/tab_interface.h"
#include "components/translate/content/browser/content_translate_driver.h"

namespace actions {
class ActionItem;
}

// PinnedTranslateActionListener observes translation events for a tab
// and updates the status indicator for the translate action item.
class PinnedTranslateActionListener
    : public translate::ContentTranslateDriver::TranslationObserver {
 public:
  explicit PinnedTranslateActionListener(tabs::TabInterface* tab);
  ~PinnedTranslateActionListener() override;

  // TranslationObserver:
  void OnIsPageTranslatedChanged(content::WebContents* source) override;

 private:
  void TabForegrounded(tabs::TabInterface* tab);
  void OnTabWillDiscardContents(tabs::TabInterface* tab,
                                content::WebContents* old_contents,
                                content::WebContents* new_contents);

  void UpdateTranslateIndicator();
  actions::ActionItem* GetTranslateActionItem(BrowserWindowInterface* browser);

  void AddTranslationObserver(content::WebContents* web_contents);
  void RemoveTranslationObserver(content::WebContents* web_contents);

  raw_ptr<tabs::TabInterface> tab_;
  std::vector<base::CallbackListSubscription> tab_subscriptions_;
  base::WeakPtrFactory<PinnedTranslateActionListener> weak_factory_{this};
};

#endif  // CHROME_BROWSER_UI_TOOLBAR_PINNED_TOOLBAR_PINNED_TRANSLATE_ACTION_LISTENER_H_
