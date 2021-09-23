// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/sharing_hub/sharing_hub_bubble_util.h"

#include "chrome/grit/generated_resources.h"
#include "components/strings/grit/components_strings.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/image_button_factory.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/grid_layout.h"
#include "ui/views/layout/layout_provider.h"

namespace sharing_hub {

namespace {

constexpr int kPaddingColumnWidth = 10;

}  // namespace

TitleWithBackButtonView::TitleWithBackButtonView(
    views::Button::PressedCallback back_callback,
    const std::u16string& window_title) {
  views::GridLayout* layout =
      SetLayoutManager(std::make_unique<views::GridLayout>());
  views::ColumnSet* columns = layout->AddColumnSet(0);

  using ColumnSize = views::GridLayout::ColumnSize;
  // Add columns for the back button, padding, and the title label.
  columns->AddColumn(views::GridLayout::LEADING, views::GridLayout::CENTER,
                     views::GridLayout::kFixedSize, ColumnSize::kUsePreferred,
                     0, 0);
  columns->AddPaddingColumn(views::GridLayout::kFixedSize, kPaddingColumnWidth);
  columns->AddColumn(views::GridLayout::FILL, views::GridLayout::CENTER, 1.f,
                     ColumnSize::kUsePreferred, 0, 0);

  layout->StartRow(views::GridLayout::kFixedSize, 0);

  auto back_button = views::CreateVectorImageButtonWithNativeTheme(
      std::move(back_callback), vector_icons::kBackArrowIcon);
  back_button->SetTooltipText(l10n_util::GetStringUTF16(IDS_TOOLTIP_BACK));
  back_button->SetAccessibleName(l10n_util::GetStringUTF16(IDS_ACCNAME_BACK));
  back_button->SizeToPreferredSize();
  InstallCircleHighlightPathGenerator(back_button.get());
  layout->AddView(std::move(back_button));

  auto title = std::make_unique<views::Label>(
      window_title, views::style::CONTEXT_DIALOG_TITLE);
  title->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  title->SetCollapseWhenHidden(true);
  title->SetMultiLine(true);
  layout->AddView(std::move(title));
}

TitleWithBackButtonView::~TitleWithBackButtonView() = default;

gfx::Size TitleWithBackButtonView::GetMinimumSize() const {
  // View::GetMinimum() defaults to GridLayout::GetPreferredSize(), but that
  // gives a larger frame width, so the dialog will become wider than it should.
  // To avoid that, just return 0x0.
  return gfx::Size(0, 0);
}

BEGIN_METADATA(TitleWithBackButtonView, views::View)
END_METADATA

}  // namespace sharing_hub
