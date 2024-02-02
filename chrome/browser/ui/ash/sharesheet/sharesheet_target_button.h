// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_SHARESHEET_SHARESHEET_TARGET_BUTTON_H_
#define CHROME_BROWSER_UI_ASH_SHARESHEET_SHARESHEET_TARGET_BUTTON_H_

#include <optional>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/sharesheet/sharesheet_types.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/gfx/vector_icon_types.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/label.h"

namespace views {
class ImageView;
}

namespace ash {
namespace sharesheet {

// SharesheetTargetButton is owned by |sharesheet_bubble_view|. It represents
// a single target (either app or action) in the |sharesheet_bubble_view|. The
// target is comprised of an image (made from |icon| for apps or from
// |vector_icon| for actions), a |display_name| and an optional
// |secondary_display_name| below it. If |is_dlp_blocked| is set to true, the
// button is disabled. Otherwise, when pressed this button launches the
// associated target.
class SharesheetTargetButton : public views::Button {
  METADATA_HEADER(SharesheetTargetButton, views::Button)

 public:
  SharesheetTargetButton(PressedCallback callback,
                         const std::u16string& display_name,
                         const std::u16string& secondary_display_name,
                         const std::optional<gfx::ImageSkia> icon,
                         const gfx::VectorIcon* vector_icon,
                         bool is_dlp_blocked);
  SharesheetTargetButton(const SharesheetTargetButton&) = delete;
  SharesheetTargetButton& operator=(const SharesheetTargetButton&) = delete;

  // views::Button:
  void OnThemeChanged() override;

 private:
  void SetLabelProperties(views::Label* label);

  raw_ptr<views::ImageView> image_;
  raw_ptr<const gfx::VectorIcon> vector_icon_;
};

}  // namespace sharesheet
}  // namespace ash

#endif  // CHROME_BROWSER_UI_ASH_SHARESHEET_SHARESHEET_TARGET_BUTTON_H_
