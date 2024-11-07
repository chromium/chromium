// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TRANSLATE_TRANSLATE_ICON_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_TRANSLATE_TRANSLATE_ICON_VIEW_H_

#include "base/scoped_observation.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/public/tab_interface.h"
#include "chrome/browser/ui/views/page_action/page_action_icon_view.h"
#include "components/translate/content/browser/content_translate_driver.h"
#include "ui/base/metadata/metadata_header_macros.h"

class CommandUpdater;

// The location bar icon to show the Translate bubble where the user can have
// the page translated.
class TranslateIconView
    : public PageActionIconView,
      public translate::ContentTranslateDriver::TranslationObserver {
  METADATA_HEADER(TranslateIconView, PageActionIconView)

 public:
  TranslateIconView(CommandUpdater* command_updater,
                    IconLabelBubbleView::Delegate* icon_label_bubble_delegate,
                    PageActionIconView::Delegate* page_action_icon_delegate,
                    Browser* browser);
  TranslateIconView(const TranslateIconView&) = delete;
  TranslateIconView& operator=(const TranslateIconView&) = delete;
  ~TranslateIconView() override;

  // PageActionIconView:
  views::BubbleDialogDelegate* GetBubble() const override;
  void UpdateImpl() override;
  bool IsBubbleShowing() const override;

 protected:
  // PageActionIconView:
  void OnExecuting(PageActionIconView::ExecuteSource execute_source) override;
  const gfx::VectorIcon& GetVectorIcon() const override;

 private:
  // Returns the Partial Translate bubble instance for the Translate icon.
  views::BubbleDialogDelegate* GetPartialTranslateBubble() const;

  // Called when the active tab changes.
  void ActiveTabChanged(BrowserWindowInterface* browser_interface);

  // Called when the active tab will be detached.
  void TabWillDetach(tabs::TabInterface* tab,
                     tabs::TabInterface::DetachReason reason);

  // translate::ContentTranslateDriver::TranslationObserver
  void OnTranslateEnabledChanged(content::WebContents* source) override;

  translate::ContentTranslateDriver* GetTranslateDriver();

  // Listens to changes to active tab.
  base::CallbackListSubscription new_active_tab_subscription_;

  // Listens to detach of active tab.
  base::CallbackListSubscription active_tab_will_detach_subscription_;

  base::ScopedObservation<
      translate::ContentTranslateDriver,
      translate::ContentTranslateDriver::TranslationObserver>
      translate_observation_{this};

  const raw_ptr<Browser> browser_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_TRANSLATE_TRANSLATE_ICON_VIEW_H_
