// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_APPS_APP_DIALOG_APP_DIALOG_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_APPS_APP_DIALOG_APP_DIALOG_VIEW_H_

#include <optional>
#include <string>

#include "base/memory/raw_ptr.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/controls/label.h"

namespace ui {
class ImageModel;
}

// The app dialog that may display the app's name, icon. This is the base class
// for app related dialog classes, e.g AppBlockDialogView, AppPauseDialogView.
class AppDialogView : public views::BubbleDialogDelegateView {
  METADATA_HEADER(AppDialogView, views::BubbleDialogDelegateView)

 public:
  explicit AppDialogView(const ui::ImageModel& image);
  ~AppDialogView() override;
  std::optional<std::u16string> GetTitleTextForTesting() const;

 protected:
  // Initializes the view with an image, a label and a close button.
  void InitializeView();

  // Can only be called after `InitializeView()`.
  void AddTitle(const std::u16string& title_text);

  // Can only be called after `InitializeView()`.
  void AddSubtitle(const std::u16string& subtitle_text);

  // Can only be called after `InitializeView()` and `AddTitle()`.
  void SetTitleText(const std::u16string& text);

  // Can only be called after `InitializeView()` and `AddSubtitle()`.
  void SetSubtitleText(const std::u16string& text);

 private:
  // BubbleDialogDelegateView:
  std::u16string GetAccessibleWindowTitle() const override;

  raw_ptr<views::Label> title_ = nullptr;
  raw_ptr<views::Label> subtitle_ = nullptr;
};

#endif  // CHROME_BROWSER_UI_VIEWS_APPS_APP_DIALOG_APP_DIALOG_VIEW_H_
