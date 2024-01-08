// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_EDITOR_MENU_EDITOR_MENU_BADGE_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_EDITOR_MENU_EDITOR_MENU_BADGE_VIEW_H_

#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/view.h"

namespace chromeos::editor_menu {

// A badge shown in the editor menu to indicate it is an experimental feature.
class EditorMenuBadgeView : public views::View {
  METADATA_HEADER(EditorMenuBadgeView, views::View)

 public:
  EditorMenuBadgeView();
  EditorMenuBadgeView(const EditorMenuBadgeView&) = delete;
  EditorMenuBadgeView& operator=(const EditorMenuBadgeView&) = delete;
  ~EditorMenuBadgeView() override;

  // views::View:
  void OnThemeChanged() override;
};

}  // namespace chromeos::editor_menu

#endif  // CHROME_BROWSER_UI_VIEWS_EDITOR_MENU_EDITOR_MENU_BADGE_VIEW_H_
