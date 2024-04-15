// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_MAHI_MAHI_CONDENSED_MENU_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_MAHI_MAHI_CONDENSED_MENU_VIEW_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/views/editor_menu/utils/pre_target_handler_view.h"
#include "ui/base/metadata/metadata_header_macros.h"

namespace views {
class LabelButton;
class ViewShadow;
}  // namespace views

namespace chromeos::mahi {

// View to show a condensed version of the Mahi Menu.
class MahiCondensedMenuView
    : public chromeos::editor_menu::PreTargetHandlerView {
  METADATA_HEADER(MahiCondensedMenuView,
                  chromeos::editor_menu::PreTargetHandlerView)

 public:
  MahiCondensedMenuView();
  MahiCondensedMenuView(const MahiCondensedMenuView&) = delete;
  MahiCondensedMenuView& operator=(const MahiCondensedMenuView&) = delete;
  ~MahiCondensedMenuView() override;

  // chromeos::editor_menu::PreTargetHandlerView:
  void RequestFocus() override;

 private:
  // Owned by the views hierarchy.
  raw_ptr<views::LabelButton> menu_button_;

  // TODO(http:/b/334748450): Remove this when `MahiCondensedMenuView` overrides
  // `ReadWriteCardsView`.
  std::unique_ptr<views::ViewShadow> view_shadow_;
};

}  // namespace chromeos::mahi

#endif  // CHROME_BROWSER_UI_VIEWS_MAHI_MAHI_CONDENSED_MENU_VIEW_H_
