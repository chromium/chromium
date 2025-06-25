// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/quick_answers/ui/magic_boost_header.h"

#include "ash/style/typography.h"
#include "chrome/browser/ui/ash/quick_answers/ui/quick_answers_util.h"
#include "chromeos/components/magic_boost/public/cpp/views/experiment_badge.h"
#include "chromeos/components/quick_answers/public/cpp/constants.h"
#include "chromeos/strings/grit/chromeos_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/layout/layout_types.h"
#include "ui/views/style/typography_provider.h"
#include "ui/views/view.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/view_utils.h"

namespace quick_answers {

views::Builder<views::BoxLayoutView> GetMagicBoostHeader() {
  int line_height = ash::TypographyProvider::Get()->ResolveLineHeight(
      ash::TypographyToken::kCrosAnnotation1);
  int vertical_padding = std::max(0, (20 - line_height) / 2);

  return views::Builder<views::BoxLayoutView>()
      .SetOrientation(views::LayoutOrientation::kHorizontal)
      .SetCrossAxisAlignment(views::LayoutAlignment::kCenter)
      .SetProperty(
          views::kFlexBehaviorKey,
          views::FlexSpecification(views::MinimumFlexSizeRule::kPreferred,
                                   views::MaximumFlexSizeRule::kPreferred))
      .SetBetweenChildSpacing(views::LayoutProvider::Get()->GetDistanceMetric(
          views::DistanceMetric::DISTANCE_RELATED_BUTTON_HORIZONTAL))
      .AddChild(
          views::Builder<views::Label>()
              .SetText(l10n_util::GetStringUTF16(IDS_ASH_MAHI_MENU_TITLE))
              .SetLineHeight(line_height)
              .SetProperty(views::kMarginsKey,
                           gfx::Insets::VH(vertical_padding, 0))
              .SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT)
              .SetFontList(ash::TypographyProvider::Get()
                               ->ResolveTypographyToken(
                                   ash::TypographyToken::kCrosAnnotation1)
                               .DeriveWithWeight(gfx::Font::Weight::MEDIUM)))
      .AddChild(views::Builder<chromeos::ExperimentBadge>());
}

}  // namespace quick_answers
