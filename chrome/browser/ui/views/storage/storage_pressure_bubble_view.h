// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_STORAGE_STORAGE_PRESSURE_BUBBLE_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_STORAGE_STORAGE_PRESSURE_BUBBLE_VIEW_H_

#include "base/memory/raw_ptr.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "url/origin.h"

class Browser;

class StoragePressureBubbleView : public views::BubbleDialogDelegateView {
  METADATA_HEADER(StoragePressureBubbleView, views::BubbleDialogDelegateView)

 public:
  static void ShowBubble(const url::Origin& origin);

 private:
  StoragePressureBubbleView(views::View* anchor_view,
                            Browser* browser,
                            const url::Origin& origin);
  ~StoragePressureBubbleView() override;

  void OnDialogAccepted();

  // views::BubbleDialogDelegateView:
  void Init() override;
  bool ShouldShowCloseButton() const override;

  const raw_ptr<Browser> browser_;
  const url::Origin origin_;
  // Whether or not the user opened the all sites page from the notification
  // positive button.
  bool ignored_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_STORAGE_STORAGE_PRESSURE_BUBBLE_VIEW_H_
