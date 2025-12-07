// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TRANSLATE_TRANSLATE_PAGE_ACTION_CONTROLLER_H_
#define CHROME_BROWSER_UI_VIEWS_TRANSLATE_TRANSLATE_PAGE_ACTION_CONTROLLER_H_

#include "base/callback_list.h"
#include "base/scoped_observation.h"
#include "chrome/browser/ui/tabs/tab_model.h"
#include "chrome/browser/ui/views/page_action/page_action_observer.h"
#include "components/tabs/public/tab_interface.h"
#include "components/translate/content/browser/content_translate_driver.h"
#include "content/public/browser/web_contents.h"

class TranslatePageActionController
    : public translate::ContentTranslateDriver::TranslationObserver,
      public page_actions::PageActionObserver {
 public:
  explicit TranslatePageActionController(tabs::TabInterface& tab_interface);
  ~TranslatePageActionController() override;
  TranslatePageActionController(const TranslatePageActionController&) = delete;
  TranslatePageActionController& operator=(
      const TranslatePageActionController&) = delete;

  // TranslationObserver
  void OnTranslateEnabledChanged(content::WebContents* source) override;

  // PageActionObserver
  void OnPageActionIconShown(
      const page_actions::PageActionState& page_action) override;
  void OnPageActionIconHidden(
      const page_actions::PageActionState& page_action) override;

 private:
  void RecordIconChange(bool showing);
  void WillDiscardContents(tabs::TabInterface* tab_interface,
                           content::WebContents* old_contents,
                           content::WebContents* new_contents);

  void UpdatePageAction();

  const raw_ref<tabs::TabInterface> tab_interface_;

  base::CallbackListSubscription will_discard_contents_subscription_;

  base::ScopedObservation<
      translate::ContentTranslateDriver,
      translate::ContentTranslateDriver::TranslationObserver>
      translate_observation_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_TRANSLATE_TRANSLATE_PAGE_ACTION_CONTROLLER_H_
