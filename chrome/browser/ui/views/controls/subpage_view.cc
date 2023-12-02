// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/controls/subpage_view.h"

#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "components/strings/grit/components_strings.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/ui_base_features.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/image_button_factory.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/controls/separator.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/flex_layout_view.h"

namespace {
constexpr int kSeparatorBottomMargin = 16;
constexpr int kBackIconSize = 16;
constexpr int kBackIconSizeRefreshStyle = 20;
constexpr int kSpaceBetweenBackArrowAndTitle = 8;
}  // namespace

DEFINE_ELEMENT_IDENTIFIER_VALUE(kSubpageViewId);

SubpageView::SubpageView(views::Button::PressedCallback callback,
                         views::BubbleFrameView* bubble_frame_view)
    : bubble_frame_view_(bubble_frame_view) {
  SetProperty(views::kElementIdentifierKey, kSubpageViewId);
  SetUpSubpageTitle(std::move(callback));
  SetLayoutManager(std::make_unique<views::FlexLayout>())
      ->SetOrientation(views::LayoutOrientation::kVertical);
}

SubpageView::~SubpageView() = default;

void SubpageView::SetTitle(const std::u16string& title) {
  title_->SetText(title);
}

void SubpageView::SetUpSubpageTitle(views::Button::PressedCallback callback) {
  auto title_view = std::make_unique<views::View>();
  title_view->SetLayoutManager(std::make_unique<views::FlexLayout>())
      ->SetCrossAxisAlignment(views::LayoutAlignment::kCenter);

  auto back_button = views::CreateVectorImageButtonWithNativeTheme(
      std::move(callback),
      features::IsChromeRefresh2023()
          ? vector_icons::kArrowBackChromeRefreshIcon
          : vector_icons::kArrowBackIcon,
      features::IsChromeRefresh2023() ? kBackIconSizeRefreshStyle
                                      : kBackIconSize);
  back_button->SetID(VIEW_ID_SUBPAGE_BACK_BUTTON);
  back_button->SetTooltipText(l10n_util::GetStringUTF16(IDS_ACCNAME_BACK));
  back_button->SetProperty(views::kInternalPaddingKey,
                           back_button->GetInsets());
  views::InstallCircleHighlightPathGenerator(back_button.get());
  title_view->AddChildView(std::move(back_button));

  title_ = title_view->AddChildView(
      views::Builder<views::Label>()
          .SetTextStyle(features::IsChromeRefresh2023()
                            ? views::style::STYLE_HEADLINE_4
                            : views::style::STYLE_SECONDARY)
          .SetTextContext(views::style::CONTEXT_DIALOG_TITLE)
          .SetHorizontalAlignment(gfx::ALIGN_LEFT)
          .Build());
  title_->SetProperty(
      views::kMarginsKey,
      gfx::Insets::TLBR(0, kSpaceBetweenBackArrowAndTitle, 0, 0));

  bubble_frame_view_->SetTitleView(std::move(title_view));
}

void SubpageView::SetContentView(std::unique_ptr<views::View> content) {
  CHECK(content);
  if (!content_view_) {
    auto* separator = AddChildView(std::make_unique<views::Separator>());
    separator->SetProperty(views::kMarginsKey,
                           gfx::Insets::TLBR(0, 0, kSeparatorBottomMargin, 0));
  } else {
    RemoveChildViewT(content_view_.ExtractAsDangling());
  }

  content_view_ = AddChildView(std::move(content));
  const int dialog_inset = views::LayoutProvider::Get()
                               ->GetInsetsMetric(views::INSETS_DIALOG)
                               .left();
  content_view_->SetProperty(views::kMarginsKey,
                             gfx::Insets::VH(0, dialog_inset));
}

void SubpageView::SetHeaderView(std::unique_ptr<views::View> header_view) {
  bubble_frame_view_->SetHeaderView(std::move(header_view));
}

void SubpageView::SetFootnoteView(std::unique_ptr<views::View> footnote_view) {
  bubble_frame_view_->SetFootnoteView(std::move(footnote_view));
}

BEGIN_METADATA(SubpageView, views::View)
END_METADATA
