// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/magic_boost/public/cpp/views/experiment_badge.h"

#include <memory>

#include "chromeos/strings/grit/chromeos_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/color/color_id.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/view.h"

namespace chromeos {

namespace {

constexpr gfx::Insets kBadgePadding = gfx::Insets::VH(0, 8);
constexpr int kBadgeCornerRadius = 9;

}  // namespace

ExperimentBadge::ExperimentBadge() {
  SetLayoutManager(std::make_unique<views::FillLayout>());

  SetBackground(views::CreateThemedRoundedRectBackground(
      ui::kColorCrosSysComplementVariant, kBadgeCornerRadius));

  label_ = AddChildView(std::make_unique<views::Label>(
      l10n_util::GetStringUTF16(IDS_EDITOR_MENU_EXPERIMENT_BADGE)));
  label_->SetEnabledColorId(ui::kColorSysOnSurface);
  label_->SetTooltipText(
      l10n_util::GetStringUTF16(IDS_MAHI_EXPERIMENT_BADGE_ACCESSIBLE_NAME));
  label_->SetBorder(views::CreateEmptyBorder(kBadgePadding));
}

ExperimentBadge::~ExperimentBadge() = default;

BEGIN_METADATA(ExperimentBadge)
END_METADATA

}  // namespace chromeos
