// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/webauthn/combined_selector_sheet_view.h"

#include <cstdlib>
#include <memory>
#include <string>
#include <utility>

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/views/accessibility/non_accessible_image_view.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/webauthn/authenticator_request_sheet_view.h"
#include "chrome/browser/ui/views/webauthn/combined_selector_views.h"
#include "chrome/browser/ui/webauthn/sheet_models.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/theme_resources.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/color/color_id.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/geometry/rounded_corners_f.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/text_constants.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/background.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/controls/table/table_view.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/layout/layout_types.h"
#include "ui/views/layout/table_layout.h"
#include "ui/views/layout/table_layout_view.h"
#include "ui/views/style/typography.h"
#include "ui/views/view.h"
#include "ui/views/view_utils.h"

namespace {

std::unique_ptr<NonAccessibleImageView> BuildIllustrationView(bool is_dark) {
  auto illustration = std::make_unique<NonAccessibleImageView>();
  illustration->SetImage(ui::ImageModel::FromResourceId(
      is_dark ? IDR_SAVE_PASSWORD_DARK : IDR_SAVE_PASSWORD));
  int preferred_width = CombinedSelectorSheetView::kIllustrationHeight *
                        illustration->GetPreferredSize().width() /
                        illustration->GetPreferredSize().height();
  illustration->SetImageSize(gfx::Size(
      preferred_width, CombinedSelectorSheetView::kIllustrationHeight));
  return illustration;
}

std::unique_ptr<views::Label> BuildTitleView(std::u16string title) {
  auto title_label =
      std::make_unique<views::Label>(title, views::style::CONTEXT_DIALOG_TITLE,
                                     views::style::STYLE_HEADLINE_4);
  title_label->SetMultiLine(true);
  title_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  title_label->GetViewAccessibility().SetRole(ax::mojom::Role::kHeading);
  title_label->SetAllowCharacterBreak(true);
  return title_label;
}

}  // namespace

CombinedSelectorSheetView::CombinedSelectorSheetView(
    std::unique_ptr<CombinedSelectorSheetModel> model)
    : AuthenticatorRequestSheetView(std::move(model)) {}

CombinedSelectorSheetView::~CombinedSelectorSheetView() = default;

std::unique_ptr<views::View>
CombinedSelectorSheetView::BuildStepSpecificHeader() {
  std::u16string title = l10n_util::GetStringFUTF16(
      IDS_WEBAUTHN_AMBIENT_BUBBLE_TITLE,
      base::UTF8ToUTF16(static_cast<CombinedSelectorSheetModel*>(model())
                            ->dialog_model()
                            ->relying_party_id));

  auto view = std::make_unique<views::TableLayoutView>();
  view->AddPaddingRow(0, kTopPadding);
  view->AddColumn(views::LayoutAlignment::kStart,
                  views::LayoutAlignment::kCenter, 1.0f,
                  views::TableLayout::ColumnSize::kUsePreferred, 0, 0);

  view->AddColumn(views::LayoutAlignment::kStart, views::LayoutAlignment::kEnd,
                  views::TableLayout::kFixedSize,
                  views::TableLayout::ColumnSize::kUsePreferred, 0, 0);
  view->AddRows(1, 0);
  // title:
  view->AddChildView(BuildTitleView(title));
  // illustration:
  view->AddChildView(BuildIllustrationView(color_utils::IsDark(
      GetColorProvider()->GetColor(ui::kColorDialogBackground))));
  return view;
}

std::pair<std::unique_ptr<views::View>,
          AuthenticatorRequestSheetView::AutoFocus>
CombinedSelectorSheetView::BuildStepSpecificContent() {
  auto scroll_view = std::make_unique<views::ScrollView>();
  scroll_view->SetContents(std::make_unique<CombinedSelectorListView>(
      static_cast<CombinedSelectorSheetModel*>(model()), this));
  scroll_view->ClipHeightTo(kMaxRowHeight, 3 * kMaxRowHeight + 2 * kRowGap);
  scroll_view->SetViewportRoundedCornerRadius(
      gfx::RoundedCornersF(ChromeLayoutProvider::Get()->GetCornerRadiusMetric(
          views::ShapeContextTokens::kComboboxRadius)));
  return std::make_pair(std::move(scroll_view), AutoFocus::kNo);
}

void CombinedSelectorSheetView::OnRadioButtonChecked(int index) {
  static_cast<CombinedSelectorSheetModel*>(model())->SetSelectionIndex(index);
}

BEGIN_METADATA(CombinedSelectorSheetView) END_METADATA
