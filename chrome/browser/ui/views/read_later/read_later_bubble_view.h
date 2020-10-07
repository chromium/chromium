// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_READ_LATER_READ_LATER_BUBBLE_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_READ_LATER_READ_LATER_BUBBLE_VIEW_H_

#include <memory>

#include "base/memory/weak_ptr.h"
#include "components/reading_list/core/reading_list_model_observer.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"

namespace views {
class WebView;
}

class Browser;

// This bubble view displays a list of read-later entries.
// This class is only used with the kReadLater feature.
class ReadLaterBubbleView : public views::BubbleDialogDelegateView,
                            public ReadingListModelObserver {
 public:
  ReadLaterBubbleView(const ReadLaterBubbleView&) = delete;
  ReadLaterBubbleView& operator=(const ReadLaterBubbleView&) = delete;

  // Displays the read-later dialog under |anchor_view|, attached to |browser|.
  static base::WeakPtr<ReadLaterBubbleView> Show(const Browser* browser,
                                                 views::View* anchor_view);

 private:
  ReadLaterBubbleView(const Browser* browser, views::View* anchor_view);

  ~ReadLaterBubbleView() override;

  // views::BubbleDialogDelegateView:
  void AddedToWidget() override;

  // ReadingListModelObserver:
  void ReadingListModelLoaded(const ReadingListModel* model) override;

  std::unique_ptr<views::WebView> const web_view_;

  base::WeakPtrFactory<ReadLaterBubbleView> weak_factory_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_READ_LATER_READ_LATER_BUBBLE_VIEW_H_
