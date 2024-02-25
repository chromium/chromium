// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_INTENT_PICKER_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_INTENT_PICKER_VIEW_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/views/page_action/page_action_icon_view.h"
#include "ui/base/metadata/metadata_header_macros.h"

class Browser;

// The entry point for the intent picker.
class IntentPickerView : public PageActionIconView {
  METADATA_HEADER(IntentPickerView, PageActionIconView)

 public:
  IntentPickerView(Browser* browser,
                   IconLabelBubbleView::Delegate* icon_label_bubble_delegate,
                   PageActionIconView::Delegate* page_action_icon_delegate);
  IntentPickerView(const IntentPickerView&) = delete;
  IntentPickerView& operator=(const IntentPickerView&) = delete;
  ~IntentPickerView() override;

  // PageActionIconView:
  void UpdateImpl() override;

 protected:
  // PageActionIconView:
  void OnExecuting(PageActionIconView::ExecuteSource execute_source) override;
  views::BubbleDialogDelegate* GetBubble() const override;
  const gfx::VectorIcon& GetVectorIcon() const override;

 private:
  bool GetShowIcon() const;

  const raw_ptr<Browser> browser_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_INTENT_PICKER_VIEW_H_
