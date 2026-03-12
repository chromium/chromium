// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/projects/projects_panel_recent_threads_expand_button.h"

#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/views/tabs/projects/layout_constants.h"
#include "chrome/browser/ui/views/tabs/projects/projects_panel_utils.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/flex_layout.h"

namespace {
constexpr int kExpandIconSize = 20;
constexpr gfx::Insets kExpandIconMargins = gfx::Insets::TLBR(0, 2, 0, 2);
}  // namespace

ProjectsPanelRecentThreadsExpandButton::ProjectsPanelRecentThreadsExpandButton(
    base::RepeatingClosure callback)
    : views::Button(std::move(callback)) {
  SetLayoutManager(std::make_unique<views::FlexLayout>())
      ->SetInteriorMargin(projects_panel::kListItemMargins)
      .SetOrientation(views::LayoutOrientation::kHorizontal)
      .SetCrossAxisAlignment(views::LayoutAlignment::kCenter);

  icon_ = AddChildView(std::make_unique<views::ImageView>());
  icon_->SetCanProcessEventsWithinSubtree(false);
  icon_->SetProperty(views::kMarginsKey, kExpandIconMargins);

  title_ = AddChildView(std::make_unique<views::Label>());
  title_->SetTextStyle(views::style::STYLE_BODY_3_MEDIUM);
  title_->SetHorizontalAlignment(gfx::ALIGN_TO_HEAD);
  title_->SetBackgroundColor(SK_ColorTRANSPARENT);
  title_->SetProperty(views::kMarginsKey,
                      projects_panel::kListItemTitleMargins);
  title_->SetProperty(
      views::kFlexBehaviorKey,
      views::FlexSpecification(views::LayoutOrientation::kHorizontal,
                               views::MinimumFlexSizeRule::kScaleToMinimum,
                               views::MaximumFlexSizeRule::kUnbounded));

  projects_panel::ConfigureInkDropForButton(this);

  SetHideInkDropWhenShowingContextMenu(true);
}

ProjectsPanelRecentThreadsExpandButton::
    ~ProjectsPanelRecentThreadsExpandButton() = default;

void ProjectsPanelRecentThreadsExpandButton::SetExpanded(bool expanded) {
  const int string_id =
      expanded ? IDS_THREADS_SHOW_LESS : IDS_THREADS_SHOW_MORE;
  const gfx::VectorIcon& icon = expanded ? kKeyboardArrowUpChromeRefreshIcon
                                         : kKeyboardArrowDownChromeRefreshIcon;

  icon_->SetImage(ui::ImageModel::FromVectorIcon(
      icon, kColorProjectsPanelButtonIcon, kExpandIconSize));
  title_->SetText(l10n_util::GetStringUTF16(string_id));
  GetViewAccessibility().SetName(l10n_util::GetStringUTF16(string_id));

  // Reset the button and ink drop state to ensure it doesn't get stuck in the
  // 'activated' or 'hovered' state if the button moves during the animation.
  SetState(STATE_NORMAL);
  auto* ink_drop = views::InkDrop::Get(this)->GetInkDrop();
  ink_drop->AnimateToState(views::InkDropState::HIDDEN);
  ink_drop->SetHovered(false);
}

BEGIN_METADATA(ProjectsPanelRecentThreadsExpandButton)
END_METADATA
