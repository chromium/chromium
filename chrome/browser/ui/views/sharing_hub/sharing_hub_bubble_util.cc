// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/sharing_hub/sharing_hub_bubble_util.h"

#include "base/functional/bind.h"
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
#include "ui/views/layout/layout_provider.h"
#include "ui/views/metadata/view_factory.h"

namespace sharing_hub {

namespace {

constexpr int kPaddingColumnWidth = 10;

}  // namespace

TitleWithBackButtonView::TitleWithBackButtonView(
    views::Button::PressedCallback back_callback,
    const std::u16string& window_title) {
  views::Builder<TitleWithBackButtonView>(this)
      .AddColumn(views::LayoutAlignment::kStart,
                 views::LayoutAlignment::kCenter,
                 views::TableLayout::kFixedSize,
                 views::TableLayout::ColumnSize::kUsePreferred, 0, 0)
      .AddPaddingColumn(views::TableLayout::kFixedSize, kPaddingColumnWidth)
      .AddColumn(views::LayoutAlignment::kStretch,
                 views::LayoutAlignment::kCenter, 1.0f,
                 views::TableLayout::ColumnSize::kUsePreferred, 0, 0)
      .AddRows(1, views::TableLayout::kFixedSize, 0)
      .AddChildren(
          views::Builder<views::ImageButton>(
              views::CreateVectorImageButtonWithNativeTheme(
                  std::move(back_callback), vector_icons::kBackArrowIcon))
              .SetTooltipText(l10n_util::GetStringUTF16(IDS_ACCNAME_BACK))
              .SetAccessibleName(l10n_util::GetStringUTF16(IDS_ACCNAME_BACK))
              .CustomConfigure(base::BindOnce([](views::ImageButton* view) {
                view->SizeToPreferredSize();
                InstallCircleHighlightPathGenerator(view);
              })),
          views::Builder<views::Label>()
              .SetText(window_title)
              .SetTextContext(views::style::CONTEXT_DIALOG_TITLE)
              .SetHorizontalAlignment(gfx::ALIGN_LEFT)
              .SetCollapseWhenHidden(true)
              .SetMultiLine(true))
      .BuildChildren();
}

TitleWithBackButtonView::~TitleWithBackButtonView() = default;

gfx::Size TitleWithBackButtonView::GetMinimumSize() const {
  // View::GetMinimum() defaults to GridLayout::GetPreferredSize(), but that
  // gives a larger frame width, so the dialog will become wider than it should.
  // To avoid that, just return 0x0.
  return gfx::Size(0, 0);
}

BEGIN_METADATA(TitleWithBackButtonView)
END_METADATA

}  // namespace sharing_hub
