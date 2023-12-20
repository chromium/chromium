// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_DOWNLOAD_BUBBLE_DOWNLOAD_BUBBLE_PASSWORD_PROMPT_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_DOWNLOAD_BUBBLE_DOWNLOAD_BUBBLE_PASSWORD_PROMPT_VIEW_H_

#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/layout/table_layout_view.h"

namespace views {
class Label;
class Textfield;
}  // namespace views

class DownloadBubblePasswordPromptView : public views::TableLayoutView {
  METADATA_HEADER(DownloadBubblePasswordPromptView, views::TableLayoutView)

 public:
  enum class State {
    kValid,
    kInvalid,
    kInvalidEmpty,
  };
  DownloadBubblePasswordPromptView();
  ~DownloadBubblePasswordPromptView() override;

  void SetState(State state);
  const std::u16string& GetText() const;

 private:
  bool IsError(State state) const;
  std::u16string GetErrorMessage(State state) const;
  std::u16string GetAccessibleName(State state) const;

  raw_ptr<views::Label> error_message_ = nullptr;
  raw_ptr<views::Textfield> password_field_ = nullptr;
};

#endif  // CHROME_BROWSER_UI_VIEWS_DOWNLOAD_BUBBLE_DOWNLOAD_BUBBLE_PASSWORD_PROMPT_VIEW_H_
