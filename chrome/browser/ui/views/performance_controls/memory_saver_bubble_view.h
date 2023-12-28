// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PERFORMANCE_CONTROLS_MEMORY_SAVER_BUBBLE_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_PERFORMANCE_CONTROLS_MEMORY_SAVER_BUBBLE_VIEW_H_

#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/performance_controls/memory_saver_bubble_observer.h"
#include "ui/views/bubble/bubble_dialog_model_host.h"
#include "ui/views/view.h"

class MemorySaverBubbleView {
 public:
  MemorySaverBubbleView(const MemorySaverBubbleView&) = delete;
  MemorySaverBubbleView& operator=(const MemorySaverBubbleView&) = delete;

  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kMemorySaverDialogBodyElementId);
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(
      kMemorySaverDialogResourceViewElementId);
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kMemorySaverDialogOkButton);
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kMemorySaverDialogCancelButton);

  static views::BubbleDialogModelHost* ShowBubble(
      Browser* browser,
      views::View* anchor_view,
      MemorySaverBubbleObserver* observer);

 private:
  MemorySaverBubbleView() = default;
};

#endif  // CHROME_BROWSER_UI_VIEWS_PERFORMANCE_CONTROLS_MEMORY_SAVER_BUBBLE_VIEW_H_
