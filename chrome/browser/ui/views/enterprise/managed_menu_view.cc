// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/enterprise/managed_menu_view.h"

#include <algorithm>
#include <string>
#include <utility>

#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/managed_ui.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/controls/hover_button.h"
#include "components/strings/grit/components_strings.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/display/screen.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/controls/separator.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/layout/table_layout.h"
#include "ui/views/style/typography.h"

namespace {
constexpr int kMenuIconSize = 16;
constexpr int kContentWidth = 280;
constexpr int kContentGap = 12;
constexpr int kContentMargin = 16;

void AddDisclaimerSection(views::View* parent,
                          const gfx::VectorIcon& vector_icon,
                          const std::u16string& text,
                          int bottom_margin = 0) {
  auto box_layout = std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal,
      gfx::Insets::TLBR(0, 0, bottom_margin, 0));
  box_layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kCenter);

  auto* browsing_data_management_section =
      parent->AddChildView(views::Builder<views::View>()
                               .SetLayoutManager(std::move(box_layout))
                               .Build());

  browsing_data_management_section->AddChildView(
      views::Builder<views::ImageView>()
          .SetImage(ui::ImageModel::FromVectorIcon(
              vector_icon, ui::kColorMenuIcon, kMenuIconSize))
          .SetBorder(
              views::CreateEmptyBorder(gfx::Insets::TLBR(0, 0, 0, kContentGap)))
          .Build());

  browsing_data_management_section->AddChildView(
      views::Builder<views::Label>()
          .SetText(text)
          .SetTextContext(views::style::CONTEXT_LABEL)
          .SetTextStyle(views::style::STYLE_BODY_4)
          .SetHorizontalAlignment(gfx::ALIGN_LEFT)
          .SetBorder(views::CreateEmptyBorder(gfx::Insets::VH(0, 0)))
          .SetMultiLine(true)
          .Build());
}

}  //  namespace

ManagedMenuView::ManagedMenuView(views::Button* anchor_button, Browser* browser)
    : views::BubbleDialogDelegateView(anchor_button,
                                      views::BubbleBorder::TOP_RIGHT),
      browser_(browser) {
  set_fixed_width(ChromeLayoutProvider::Get()->GetDistanceMetric(
      views::DISTANCE_BUBBLE_PREFERRED_WIDTH));
  SetButtons(ui::DIALOG_BUTTON_OK);
  SetDefaultButton(ui::DIALOG_BUTTON_NONE);
  GetViewAccessibility().SetRole(ax::mojom::Role::kDialog);
  GetViewAccessibility().SetName(GetAccessibleWindowTitle());
  SetButtonLabel(ui::DIALOG_BUTTON_OK,
                 l10n_util::GetStringUTF16(IDS_LEARN_MORE));
  SetAcceptCallback(base::BindOnce(
      &chrome::ShowEnterpriseManagementPageInTabbedBrowser, browser_));
  SetShowCloseButton(true);
  SetTitle(chrome::GetManagementBubbleTitle(browser_->profile()));
  SetShowIcon(true);
  SetIcon(ui::ImageModel::FromVectorIcon(vector_icons::kBusinessIcon,
                                         ui::kColorMenuIcon, 24));
}

ManagedMenuView::~ManagedMenuView() = default;

void ManagedMenuView::Init() {
  RemoveAllChildViews();
  BuildView();
}

int ManagedMenuView::GetMaxHeight() const {
  gfx::Rect anchor_rect = GetAnchorRect();
  gfx::Rect screen_space =
      display::Screen::GetScreen()
          ->GetDisplayNearestPoint(anchor_rect.CenterPoint())
          .work_area();
  int available_space = screen_space.bottom() - anchor_rect.bottom();
#if BUILDFLAG(IS_WIN)
  // On Windows the bubble can also be shown to the top of the anchor.
  available_space =
      std::max(available_space, anchor_rect.y() - screen_space.y());
#endif
  return std::max(0, available_space);
}

void ManagedMenuView::BuildView() {
  // Create a table layout to set the menu width.
  SetLayoutManager(std::make_unique<views::TableLayout>())
      ->AddColumn(
          views::LayoutAlignment::kStretch, views::LayoutAlignment::kStretch,
          views::TableLayout::kFixedSize,
          views::TableLayout::ColumnSize::kFixed, kContentWidth, kContentWidth)
      .AddRows(1, 1.0f);

  // Create a scroll view to hold the components.
  auto* scroll_view = AddChildView(std::make_unique<views::ScrollView>());
  scroll_view->SetHorizontalScrollBarMode(
      views::ScrollView::ScrollBarMode::kDisabled);
  // TODO(crbug.com/41406562): it's a workaround for the crash.
  scroll_view->SetDrawOverflowIndicator(false);
  scroll_view->ClipHeightTo(0, GetMaxHeight());

  info_container_ = scroll_view->SetContents(std::make_unique<views::View>());

  auto box_layout = std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical,
      gfx::Insets::VH(kContentMargin, kContentMargin));
  box_layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kStart);
  info_container_->SetLayoutManager(std::move(box_layout));

  // Things to consider section
  info_container_->AddChildView(
      views::Builder<views::Label>()
          .SetText(l10n_util::GetStringUTF16(
              IDS_MANAGEMENT_DIALOG_THINGS_TO_CONSIDER_SUBTITLE))
          .SetTextContext(views::style::CONTEXT_LABEL)
          .SetTextStyle(views::style::STYLE_BODY_4_BOLD)
          .SetHorizontalAlignment(gfx::ALIGN_LEFT)
          .SetBorder(
              views::CreateEmptyBorder(gfx::Insets::TLBR(0, 0, kContentGap, 0)))
          .SetMultiLine(true)
          .Build());

  AddDisclaimerSection(
      info_container_, kPrivacyTipIcon,
      l10n_util::GetStringUTF16(IDS_MANAGEMENT_DIALOG_BROWSING_DATA_MANAGEMENT),
      /*bottom_marging*/ kContentGap);
  AddDisclaimerSection(
      info_container_, kDevicesIcon,
      l10n_util::GetStringUTF16(IDS_MANAGEMENT_DIALOG_DEVICE_SIGNALS));

  info_container_background_callback_ = base::BindRepeating(
      &ManagedMenuView::BuildInfoContainerBackground, base::Unretained(this));
}

std::u16string ManagedMenuView::GetAccessibleWindowTitle() const {
  return chrome::GetManagedUiMenuItemLabel(browser_->profile());
}

void ManagedMenuView::OnThemeChanged() {
  views::BubbleDialogDelegateView::OnThemeChanged();
  const auto* color_provider = GetColorProvider();
  info_container_background_callback_.Run(color_provider);
}

void ManagedMenuView::BuildInfoContainerBackground(
    const ui::ColorProvider* color_provider) {
  const int radius = views::LayoutProvider::Get()->GetCornerRadiusMetric(
      views::Emphasis::kHigh);
  info_container_->SetBackground(views::CreateRoundedRectBackground(
      color_provider->GetColor(ui::kColorSysSurface4), radius));
  info_container_->SetBorder(views::CreatePaddedBorder(
      views::CreateRoundedRectBorder(
          0, radius, color_provider->GetColor(ui::kColorSysSurface4)),
      gfx::Insets::VH(0, 0)));
}

BEGIN_METADATA(ManagedMenuView)
END_METADATA
