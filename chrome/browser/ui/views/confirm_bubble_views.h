// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_CONFIRM_BUBBLE_VIEWS_H_
#define CHROME_BROWSER_UI_VIEWS_CONFIRM_BUBBLE_VIEWS_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/window/dialog_delegate.h"

class ConfirmBubbleModel;

namespace views {
class Label;
}  // namespace views

// A dialog (with the standard Title/[OK]/[Cancel] UI elements), as well as
// a message Label and help (?) button. The dialog ultimately appears like this:
//   +------------------------+
//   | Title                  |
//   | Label                  |
//   | (?)      [OK] [Cancel] |
//   +------------------------+
//
// TODO(msw): Remove this class or merge it with DialogDelegateView.
class ConfirmBubbleViews : public views::DialogDelegateView {
  METADATA_HEADER(ConfirmBubbleViews, views::DialogDelegateView)

 public:
  explicit ConfirmBubbleViews(std::unique_ptr<ConfirmBubbleModel> model);
  ConfirmBubbleViews(const ConfirmBubbleViews&) = delete;
  ConfirmBubbleViews& operator=(const ConfirmBubbleViews&) = delete;

 protected:
  ~ConfirmBubbleViews() override;

  // views::DialogDelegateView:
  std::u16string GetWindowTitle() const override;
  bool ShouldShowCloseButton() const override;
  void OnWidgetInitialized() override;

 private:
  // The model to customize this bubble view.
  std::unique_ptr<ConfirmBubbleModel> model_;

  raw_ptr<views::Label> label_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_CONFIRM_BUBBLE_VIEWS_H_
