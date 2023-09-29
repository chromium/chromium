// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_EDITOR_MENU_EDITOR_MENU_GRADIENT_BADGE_H_
#define CHROME_BROWSER_UI_VIEWS_EDITOR_MENU_EDITOR_MENU_GRADIENT_BADGE_H_

#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/view.h"

namespace chromeos::editor_menu {

// A badge with a gradient background which is shown in the editor menu to
// indicate it is an experimental feature.
class EditorMenuGradientBadge : public views::View {
 public:
  METADATA_HEADER(EditorMenuGradientBadge);

  EditorMenuGradientBadge();
  EditorMenuGradientBadge(const EditorMenuGradientBadge&) = delete;
  EditorMenuGradientBadge& operator=(const EditorMenuGradientBadge&) = delete;
  ~EditorMenuGradientBadge() override;

  // View:
  gfx::Size CalculatePreferredSize() const override;
  void OnPaint(gfx::Canvas* canvas) override;
};

}  // namespace chromeos::editor_menu

#endif  // CHROME_BROWSER_UI_VIEWS_EDITOR_MENU_EDITOR_MENU_GRADIENT_BADGE_H_
