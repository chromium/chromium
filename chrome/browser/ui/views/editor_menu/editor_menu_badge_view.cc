// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/editor_menu/editor_menu_badge_view.h"

#include "chrome/browser/ui/views/editor_menu/editor_menu_strings.h"
#include "chromeos/strings/grit/chromeos_strings.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/font.h"
#include "ui/gfx/font_list.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/skia_conversions.h"
#include "ui/gfx/skia_paint_util.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/view.h"
#include "ui/views/view_class_properties.h"

namespace chromeos::editor_menu {

namespace {

// TODO(b/302209940): Replace these with color tokens.
constexpr SkColor kBadgeBackgroundColorLight = SkColorSetRGB(0xC1, 0xFE, 0xE2);
constexpr SkColor kBadgeBackgroundColorDark = SkColorSetRGB(0x13, 0x50, 0x3D);

constexpr int kBadgeFontSize = 10;
constexpr int kBadgeLineHeight = kBadgeFontSize;
const gfx::FontList kBadgeFont({"Roboto", "Google Sans"},
                               gfx::Font::NORMAL,
                               kBadgeFontSize,
                               gfx::Font::Weight::MEDIUM);

constexpr gfx::Insets kBadgeInsets = gfx::Insets::VH(4, 8);

}  // namespace

EditorMenuBadgeView::EditorMenuBadgeView() {
  SetLayoutManager(std::make_unique<views::FillLayout>());

  auto* label = AddChildView(
      std::make_unique<views::Label>(GetEditorMenuExperimentBadgeLabel(),
                                     views::Label::CustomFont({kBadgeFont})));
  label->SetEnabledColorId(ui::kColorSysOnSurface);
  label->SetLineHeight(kBadgeLineHeight);
  label->SetBorder(views::CreateEmptyBorder(kBadgeInsets));
}

EditorMenuBadgeView::~EditorMenuBadgeView() = default;

void EditorMenuBadgeView::OnThemeChanged() {
  views::View::OnThemeChanged();

  SetBackground(views::CreateRoundedRectBackground(
      color_utils::IsDark(
          GetColorProvider()->GetColor(ui::kColorPrimaryBackground))
          ? kBadgeBackgroundColorDark
          : kBadgeBackgroundColorLight,
      GetPreferredSize().height() / 2));
}

BEGIN_METADATA(EditorMenuBadgeView)
END_METADATA

}  // namespace chromeos::editor_menu
