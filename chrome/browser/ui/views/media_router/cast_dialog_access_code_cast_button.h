// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_MEDIA_ROUTER_CAST_DIALOG_ACCESS_CODE_CAST_BUTTON_H_
#define CHROME_BROWSER_UI_VIEWS_MEDIA_ROUTER_CAST_DIALOG_ACCESS_CODE_CAST_BUTTON_H_

#include "base/gtest_prod_util.h"
#include "chrome/browser/ui/views/controls/hover_button.h"
#include "ui/base/metadata/metadata_header_macros.h"

namespace ui {
class MouseEvent;
}

namespace media_router {

// A button representing to launch the Access code cast dialog.
class CastDialogAccessCodeCastButton : public HoverButton {
  METADATA_HEADER(CastDialogAccessCodeCastButton, HoverButton)

 public:
  explicit CastDialogAccessCodeCastButton(PressedCallback callback);
  CastDialogAccessCodeCastButton(const CastDialogAccessCodeCastButton&) =
      delete;
  CastDialogAccessCodeCastButton& operator=(
      const CastDialogAccessCodeCastButton&) = delete;
  ~CastDialogAccessCodeCastButton() override;

  // views::View:
  bool OnMousePressed(const ui::MouseEvent& event) override;
  void OnMouseReleased(const ui::MouseEvent& event) override;

 private:
  friend class MediaRouterUiForTest;
  FRIEND_TEST_ALL_PREFIXES(CastDialogAccessCodeCastButtonTest, DefaultText);
  FRIEND_TEST_ALL_PREFIXES(CastDialogAccessCodeCastButtonTest, AddDeviceText);
};

}  // namespace media_router

#endif  // CHROME_BROWSER_UI_VIEWS_MEDIA_ROUTER_CAST_DIALOG_ACCESS_CODE_CAST_BUTTON_H_
