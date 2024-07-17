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
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/layout/flex_layout_view.h"

namespace {
constexpr int kSeparatorBottomMargin = 16;
constexpr int kBackIconSize = 20;
}  // namespace

DEFINE_ELEMENT_IDENTIFIER_VALUE(kSubpageViewId);
DEFINE_ELEMENT_IDENTIFIER_VALUE(kSubpageBackButtonElementId);

SubpageView::SubpageView(views::Button::PressedCallback callback,
                         views::BubbleFrameView* bubble_frame_view)
    : bubble_frame_view_(bubble_frame_view) {
  SetProperty(views::kElementIdentifierKey, kSubpageViewId);
  SetUpSubpageTitle(std::move(callback));
  SetLayoutManager(std::make_unique<views::FlexLayout>())
      ->SetOrientation(views::LayoutOrientation::kVertical);
}

SubpageView::~SubpageView() {
  if (title_) {
    title_->RemoveObserver(this);
  }
}

void SubpageView::SetTitle(const std::u16string& title) {
  title_->SetText(title);
}

void SubpageView::SetUpSubpageTitle(views::Button::PressedCallback callback) {
  const auto* layout_provider = ChromeLayoutProvider::Get();
  auto title_view = std::make_unique<views::BoxLayoutView>();
  title_view->SetBetweenChildSpacing(
      layout_provider->GetDistanceMetric(
          views::DISTANCE_RELATED_CONTROL_HORIZONTAL) -
      layout_provider->GetInsetsMetric(views::INSETS_VECTOR_IMAGE_BUTTON)
          .right());

  auto back_button = views::CreateVectorImageButtonWithNativeTheme(
      std::move(callback), vector_icons::kArrowBackChromeRefreshIcon,
      kBackIconSize);
  back_button->SetID(VIEW_ID_SUBPAGE_BACK_BUTTON);
  back_button->SetTooltipText(l10n_util::GetStringUTF16(IDS_ACCNAME_BACK));
  back_button->SetProperty(views::kInternalPaddingKey,
                           back_button->GetInsets());
  back_button->SetProperty(views::kElementIdentifierKey,
                           kSubpageBackButtonElementId);
  views::InstallCircleHighlightPathGenerator(back_button.get());
  title_view->AddChildView(std::move(back_button));

  title_ = title_view->AddChildView(
      views::Builder<views::Label>()
          .SetTextStyle(views::style::STYLE_HEADLINE_4)
          .SetTextContext(views::style::CONTEXT_DIALOG_TITLE)
          .SetHorizontalAlignment(gfx::ALIGN_LEFT)
          .Build());
  title_->SetMultiLine(true);
  // This limits the SubpageView only works for standard the preferred width
  // bubble.
  int title_width =
      layout_provider->GetDistanceMetric(
          views::DISTANCE_BUBBLE_PREFERRED_WIDTH) -
      layout_provider->GetInsetsMetric(views::INSETS_DIALOG).width() -
      layout_provider->GetInsetsMetric(views::INSETS_DIALOG_TITLE).width();
  views::Button* close_button = bubble_frame_view_->close_button();
  if (close_button && close_button->GetVisible()) {
    int close_button_width =
        bubble_frame_view_->close_button()->width() +
        layout_provider->GetDistanceMetric(views::DISTANCE_CLOSE_BUTTON_MARGIN);
    title_width -= close_button_width;
  }
  title_->SetMaximumWidth(title_width);
  // We need to observe the `title_` view for destruction in order to clear the
  // raw_ptr to prevent a dangling reference. This is because the `title_` is
  // owned by a view other than this view. That other view is destroyed prior
  // to the destruction of this view.
  title_->AddObserver(this);

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

void SubpageView::OnViewIsDeleting(views::View* view) {
  if (view == title_.get()) {
    title_->RemoveObserver(this);
    title_ = nullptr;
  }
}

BEGIN_METADATA(SubpageView)
END_METADATA
