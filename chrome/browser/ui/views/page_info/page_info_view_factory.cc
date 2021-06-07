// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/page_info/page_info_view_factory.h"

#include "chrome/browser/ui/page_info/chrome_page_info_ui_delegate.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/page_info/page_info_main_view.h"
#include "chrome/browser/ui/views/page_info/page_info_navigation_handler.h"
#include "chrome/browser/ui/views/page_info/page_info_security_content_view.h"
#include "components/page_info/page_info.h"
#include "components/strings/grit/components_strings.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/image_button_factory.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/controls/separator.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/layout/grid_layout.h"

// static
std::unique_ptr<views::View> PageInfoViewFactory::CreateSeparator() {
  // Distance for multi content list is used, but split in half, since there is
  // a separator in the middle of it.
  const int separator_spacing = ChromeLayoutProvider::Get()->GetDistanceMetric(
                                    DISTANCE_CONTENT_LIST_VERTICAL_MULTI) /
                                2;
  auto separator = std::make_unique<views::Separator>();
  separator->SetProperty(views::kMarginsKey, gfx::Insets(separator_spacing, 0));
  return separator;
}

// static
std::unique_ptr<views::View> PageInfoViewFactory::CreateLabelWrapper() {
  const int icon_label_spacing = ChromeLayoutProvider::Get()->GetDistanceMetric(
      views::DISTANCE_RELATED_LABEL_HORIZONTAL);
  auto label_wrapper = std::make_unique<views::View>();
  label_wrapper->SetLayoutManager(std::make_unique<views::FlexLayout>())
      ->SetOrientation(views::LayoutOrientation::kVertical);
  label_wrapper->SetProperty(views::kMarginsKey,
                             gfx::Insets(0, icon_label_spacing));
  label_wrapper->SetProperty(views::kCrossAxisAlignmentKey,
                             views::LayoutAlignment::kStretch);
  label_wrapper->SetProperty(
      views::kFlexBehaviorKey,
      views::FlexSpecification(views::MinimumFlexSizeRule::kScaleToZero,
                               views::MaximumFlexSizeRule::kUnbounded,
                               /*adjust_height_for_width =*/true)
          .WithWeight(1));
  return label_wrapper;
}

PageInfoViewFactory::PageInfoViewFactory(
    PageInfo* presenter,
    ChromePageInfoUiDelegate* ui_delegate,
    PageInfoNavigationHandler* navigation_handler)
    : presenter_(presenter),
      ui_delegate_(ui_delegate),
      navigation_handler_(navigation_handler) {}

std::unique_ptr<views::View> PageInfoViewFactory::CreateMainPageView() {
  return std::make_unique<PageInfoMainView>(presenter_, ui_delegate_,
                                            navigation_handler_);
}

std::unique_ptr<views::View> PageInfoViewFactory::CreateSecurityPageView() {
  return CreateSubpage(CreateSubpageHeader(l10n_util::GetStringUTF16(
                           IDS_PAGE_INFO_SECURITY_SUBPAGE_HEADER)),
                       std::make_unique<PageInfoSecurityContentView>(
                           presenter_, /*is_standalone_page=*/true));
}

std::unique_ptr<views::View> PageInfoViewFactory::CreateSubpage(
    std::unique_ptr<views::View> header,
    std::unique_ptr<views::View> content) {
  auto subpage = std::make_unique<views::View>();
  subpage->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical));
  subpage->AddChildView(std::move(header));
  subpage->AddChildView(std::move(content));
  return subpage;
}

std::unique_ptr<views::View> PageInfoViewFactory::CreateSubpageHeader(
    std::u16string title) {
  ChromeLayoutProvider* layout_provider = ChromeLayoutProvider::Get();
  auto wrapper = std::make_unique<views::View>();
  wrapper->SetLayoutManager(std::make_unique<views::FlexLayout>())
      ->SetOrientation(views::LayoutOrientation::kVertical);

  const int side_margin =
      layout_provider->GetInsetsMetric(views::INSETS_DIALOG).left();
  const int bottom_margin =
      layout_provider->GetDistanceMetric(DISTANCE_CONTENT_LIST_VERTICAL_MULTI);

  auto* header = wrapper->AddChildView(std::make_unique<views::View>());
  header->SetLayoutManager(std::make_unique<views::FlexLayout>())
      ->SetCrossAxisAlignment(views::LayoutAlignment::kStart)
      .SetInteriorMargin(
          gfx::Insets(0, side_margin, bottom_margin, side_margin));
  wrapper->AddChildView(CreateSeparator());

  auto back_button = views::CreateVectorImageButtonWithNativeTheme(
      base::BindRepeating(&PageInfoNavigationHandler::OpenMainPage,
                          base::Unretained(navigation_handler_)),
      vector_icons::kBackArrowIcon);
  views::InstallCircleHighlightPathGenerator(back_button.get());
  back_button->SetProperty(views::kInternalPaddingKey,
                           back_button->GetInsets());
  header->AddChildView(std::move(back_button));

  auto* label_wrapper = header->AddChildView(CreateLabelWrapper());
  auto* title_label = label_wrapper->AddChildView(
      std::make_unique<views::Label>(title, views::style::CONTEXT_DIALOG_TITLE,
                                     views::style::STYLE_SECONDARY));
  title_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  auto* subtitle_label =
      label_wrapper->AddChildView(std::make_unique<views::Label>(
          presenter_->GetSimpleSiteName(), views::style::CONTEXT_LABEL,
          views::style::STYLE_SECONDARY));
  subtitle_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);

  auto close_button = views::BubbleFrameView::CreateCloseButton(
      base::BindRepeating(&PageInfoNavigationHandler::CloseBubble,
                          base::Unretained(navigation_handler_)));
  close_button->SetVisible(true);
  close_button->SetProperty(views::kInternalPaddingKey,
                            close_button->GetInsets());
  header->AddChildView(std::move(close_button));

  return wrapper;
}
