// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TRANSLATE_BROWSER_ACTION_LISTENER_H_
#define CHROME_BROWSER_UI_TRANSLATE_BROWSER_ACTION_LISTENER_H_

#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "components/translate/content/browser/content_translate_driver.h"

class Browser;

class TranslateBrowserActionListener final
    : public TabStripModelObserver,
      public translate::ContentTranslateDriver::TranslationObserver {
 public:
  explicit TranslateBrowserActionListener(Browser& browser);
  TranslateBrowserActionListener(const TranslateBrowserActionListener&) =
      delete;
  TranslateBrowserActionListener& operator=(
      const TranslateBrowserActionListener&) = delete;
  ~TranslateBrowserActionListener() final;

  // Overridden from TabStripModelObserver:
  void OnTabStripModelChanged(
      TabStripModel* tab_strip_model,
      const TabStripModelChange& change,
      const TabStripSelectionChange& selection) override;

  // Overridden from translate::ContentTranslateDriver::TranslationObserver:
  void OnIsPageTranslatedChanged(content::WebContents* source) override;

 private:
  void AddTranslationObserver(content::WebContents* web_contents);
  void RemoveTranslationObserver(content::WebContents* web_contents);
  const raw_ref<Browser> browser_;
};

#endif  // CHROME_BROWSER_UI_TRANSLATE_BROWSER_ACTION_LISTENER_H_
