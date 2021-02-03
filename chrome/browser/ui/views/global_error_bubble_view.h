// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_GLOBAL_ERROR_BUBBLE_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_GLOBAL_ERROR_BUBBLE_VIEW_H_

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/global_error/global_error_bubble_view_base.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/metadata/metadata_header_macros.h"

class Browser;
class ElevationIconSetter;
class GlobalErrorWithStandardBubble;

class GlobalErrorBubbleView : public views::BubbleDialogDelegateView,
                              public GlobalErrorBubbleViewBase {
 public:
  METADATA_HEADER(GlobalErrorBubbleView);
  GlobalErrorBubbleView(
      views::View* anchor_view,
      views::BubbleBorder::Arrow arrow,
      Browser* browser,
      const base::WeakPtr<GlobalErrorWithStandardBubble>& error);
  GlobalErrorBubbleView(const GlobalErrorBubbleView&) = delete;
  GlobalErrorBubbleView& operator=(const GlobalErrorBubbleView&) = delete;
  ~GlobalErrorBubbleView() override;

  // views::BubbleDialogDelegateView implementation.
  void Init() override;
  void OnDialogInitialized() override;

  // GlobalErrorBubbleViewBase implementation.
  void CloseBubbleView() override;

 private:
  base::WeakPtr<GlobalErrorWithStandardBubble> error_;

  std::unique_ptr<ElevationIconSetter> elevation_icon_setter_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_GLOBAL_ERROR_BUBBLE_VIEW_H_
