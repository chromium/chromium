// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/mahi/public/cpp/views/experiment_badge.h"

#include <memory>

#include "chromeos/strings/grit/chromeos_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/view.h"

namespace chromeos::mahi {

namespace {

constexpr gfx::Insets kBadgePadding = gfx::Insets::VH(3, 8);
constexpr int kBadgeCornerRadius = 9;
constexpr int kBadgeLabelHeight = 12;

}  // namespace

ExperimentBadge::ExperimentBadge() {
  SetLayoutManager(std::make_unique<views::FillLayout>());

  SetBackground(views::CreateThemedRoundedRectBackground(
      cros_tokens::kCrosSysComplementVariant, kBadgeCornerRadius));

  label_ = AddChildView(std::make_unique<views::Label>(
      l10n_util::GetStringUTF16(IDS_EDITOR_MENU_EXPERIMENT_BADGE)));
  label_->SetEnabledColorId(cros_tokens::kCrosSysOnSurface);
  label_->SetLineHeight(kBadgeLabelHeight);
  label_->SetTooltipText(
      l10n_util::GetStringUTF16(IDS_MAHI_EXPERIMENT_BADGE_ACCESSIBLE_NAME));
  label_->SetBorder(views::CreateEmptyBorder(kBadgePadding));

  SetAccessibleName(
      l10n_util::GetStringUTF16(IDS_MAHI_EXPERIMENT_BADGE_ACCESSIBLE_NAME));
}

ExperimentBadge::~ExperimentBadge() = default;

BEGIN_METADATA(ExperimentBadge)
END_METADATA

}  // namespace chromeos::mahi
