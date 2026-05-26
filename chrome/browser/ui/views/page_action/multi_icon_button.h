// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PAGE_ACTION_MULTI_ICON_BUTTON_H_
#define CHROME_BROWSER_UI_VIEWS_PAGE_ACTION_MULTI_ICON_BUTTON_H_

#include <functional>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/label.h"

namespace page_actions {

// This button shows one or more icons on it, up to a limit.
class MultiIconButton : public views::Button {
  METADATA_HEADER(MultiIconButton, views::Button)
 public:
  explicit MultiIconButton(PressedCallback callback);
  MultiIconButton(const MultiIconButton&) = delete;
  MultiIconButton& operator=(const MultiIconButton&) = delete;
  ~MultiIconButton() override;

  // views::Button:
  // GetChildrenInZOrder() is overridden to help show overlapping icons.
  views::View::Views GetChildrenInZOrder() override;
  void OnThemeChanged() override;

  void Update(
      const std::vector<std::reference_wrapper<const ui::ImageModel>>& icons);

 private:
  // Label for trailing text indicating how many more un-shown items there are.
  // For example, if 10 items are supplied and the number of displayed icons is
  // 3, the label will show "+7".
  raw_ptr<views::Label> plus_more_label_ = nullptr;
};

}  // namespace page_actions

#endif  // CHROME_BROWSER_UI_VIEWS_PAGE_ACTION_MULTI_ICON_BUTTON_H_
