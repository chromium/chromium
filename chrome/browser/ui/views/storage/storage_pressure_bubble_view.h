// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_STORAGE_STORAGE_PRESSURE_BUBBLE_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_STORAGE_STORAGE_PRESSURE_BUBBLE_VIEW_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "url/origin.h"

class BrowserWindowInterface;

class StoragePressureBubbleView : public views::BubbleDialogDelegateView {
  METADATA_HEADER(StoragePressureBubbleView, views::BubbleDialogDelegateView)

 public:
  static void ShowBubble(const url::Origin& origin);

 private:
  StoragePressureBubbleView(views::View* anchor_view,
                            BrowserWindowInterface* browser,
                            const url::Origin& origin);
  ~StoragePressureBubbleView() override;

  void OnDialogAccepted();

  // views::BubbleDialogDelegateView:
  void Init() override;
  bool ShouldShowCloseButton() const override;

  const raw_ptr<BrowserWindowInterface> bwi_;
  const url::Origin origin_;
  // Whether or not the user opened the all sites page from the notification
  // positive button.
  bool ignored_;

  // TODO(https://crbug.com/372479681): Remove these two members and all uses of
  // them. They are here for debugging a crash we can't reproduce under
  // controlled conditions.
  bool in_accept_ = false;
  base::WeakPtrFactory<StoragePressureBubbleView> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_STORAGE_STORAGE_PRESSURE_BUBBLE_VIEW_H_
