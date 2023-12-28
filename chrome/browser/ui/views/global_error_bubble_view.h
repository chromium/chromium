// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_GLOBAL_ERROR_BUBBLE_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_GLOBAL_ERROR_BUBBLE_VIEW_H_

#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/global_error/global_error_bubble_view_base.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"

class Browser;
class ElevationIconSetter;
class GlobalErrorWithStandardBubble;

class GlobalErrorBubbleView : public views::BubbleDialogDelegateView,
                              public GlobalErrorBubbleViewBase {
  METADATA_HEADER(GlobalErrorBubbleView, views::BubbleDialogDelegateView)

 public:
  GlobalErrorBubbleView(
      views::View* anchor_view,
      views::BubbleBorder::Arrow arrow,
      Browser* browser,
      const base::WeakPtr<GlobalErrorWithStandardBubble>& error);
  GlobalErrorBubbleView(const GlobalErrorBubbleView&) = delete;
  GlobalErrorBubbleView& operator=(const GlobalErrorBubbleView&) = delete;
  ~GlobalErrorBubbleView() override;

  // views::BubbleDialogDelegateView:
  void Init() override;
  void OnWidgetInitialized() override;

  // GlobalErrorBubbleViewBase:
  void CloseBubbleView() override;

 private:
  base::WeakPtr<GlobalErrorWithStandardBubble> error_;

  std::unique_ptr<ElevationIconSetter> elevation_icon_setter_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_GLOBAL_ERROR_BUBBLE_VIEW_H_
