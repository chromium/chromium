// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/bookmarks/bookmark_menu_button_base.h"

#include <memory>
#include <string_view>

#include "chrome/browser/ui/views/bookmarks/bookmark_button_util.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/toolbar/toolbar_ink_drop_util.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/button_controller.h"
#include "ui/views/controls/button/label_button_border.h"
#include "ui/views/controls/button/menu_button.h"
#include "ui/views/controls/button/menu_button_controller.h"
#include "ui/views/controls/highlight_path_generator.h"

namespace {

// The `MenuButtonControllerDelegate` is used to determine whether a button
// can be in a hover state. Regardless of where the drag originates (it could
// be the bookmarks bar, a folder submenu triggered via
// `BookmarkFolderButton`, another browser window, or even another Chrome
// process), the menu button must not be in a hover state when it has no last
// pressed lock in `MenuButtonController`.
// See https://crbug.com/476761940
class MenuButtonControllerDelegate
    : public views::Button::DefaultButtonControllerDelegate {
 public:
  explicit MenuButtonControllerDelegate(
      views::Button* button,
      BookmarkMenuButtonBase::IsDraggingCallback is_dragging_callback)
      : views::Button::DefaultButtonControllerDelegate(button),
        is_dragging_callback_(is_dragging_callback) {}
  ~MenuButtonControllerDelegate() override = default;

 private:
  // DefaultButtonControllerDelegate:
  bool ShouldEnterHoveredState() override {
    // When a bookmark is being dragged, it should not be set to the
    // `Button::STATE_HOVERED` state. Otherwise, this hover state will persist
    // after the menu is dismissed.
    bool is_dragging = is_dragging_callback_ && is_dragging_callback_.Run();
    return !is_dragging && views::Button::DefaultButtonControllerDelegate::
                               ShouldEnterHoveredState();
  }

  BookmarkMenuButtonBase::IsDraggingCallback is_dragging_callback_;
};

}  // namespace

BookmarkMenuButtonBase::BookmarkMenuButtonBase(
    PressedCallback callback,
    IsDraggingCallback is_dragging_callback,
    std::u16string_view title)
    : MenuButton(PressedCallback(), title) {
  ConfigureInkDropForToolbar(this);
  SetImageLabelSpacing(ChromeLayoutProvider::Get()->GetDistanceMetric(
      DISTANCE_RELATED_LABEL_HORIZONTAL_LIST));
  views::InstallPillHighlightPathGenerator(this);

  SetMenuButtonController(std::make_unique<views::MenuButtonController>(
      this, std::move(callback),
      std::make_unique<MenuButtonControllerDelegate>(
          this, std::move(is_dragging_callback))));
}

BookmarkMenuButtonBase::~BookmarkMenuButtonBase() = default;

// MenuButton:
std::unique_ptr<views::LabelButtonBorder>
BookmarkMenuButtonBase::CreateDefaultBorder() const {
  return bookmark_button_util::CreateBookmarkButtonBorder();
}

BEGIN_METADATA(BookmarkMenuButtonBase)
END_METADATA
