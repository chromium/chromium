// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/performance_controls/tab_list_row_view.h"

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/time/time.h"
#include "chrome/browser/ui/performance_controls/tab_list_model.h"
#include "chrome/browser/ui/tab_ui_helper.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "components/performance_manager/public/resource_attribution/page_context.h"
#include "components/strings/grit/components_strings.h"
#include "components/url_formatter/url_formatter.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/color/color_id.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/animation/ink_drop_host.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/image_button_factory.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/layout/layout_types.h"
#include "ui/views/style/typography.h"
#include "ui/views/vector_icons.h"
#include "ui/views/view.h"
#include "ui/views/view_class_properties.h"
#include "url/gurl.h"

namespace {
// Spacing placed on top and below the favicon so that the favicon border
// won't touch the edge of the TabListRowView.
constexpr int kFaviconVerticalMargin = 4;
// Corner radius for the favicon image view.
constexpr int kFaviconCornerRadius = 8;
// Border thickness surrounding the favicon.
constexpr int kFaviconBorderThickness = 12;

std::unique_ptr<views::Label> CreateLabel(std::u16string text, int text_style) {
  auto label = std::make_unique<views::Label>(text);

  label->SetMultiLine(false);
  label->SetMaxLines(1);
  label->SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT);
  label->SetTextStyle(text_style);
  label->SetProperty(
      views::kFlexBehaviorKey,
      views::FlexSpecification(views::LayoutOrientation::kHorizontal,
                               views::MinimumFlexSizeRule::kScaleToMinimum,
                               views::MaximumFlexSizeRule::kUnbounded, true));

  return label;
}
}  // namespace

TabListRowView::TabListRowView(
    resource_attribution::PageContext tab,
    TabListModel* tab_list_model,
    base::OnceCallback<void(TabListRowView*)> close_button_callback)
    : actionable_tab_(tab),
      tab_list_model_(tab_list_model),
      inkdrop_container_(
          AddChildView(std::make_unique<views::InkDropContainerView>())) {
  views::FlexLayout* const flex_layout =
      views::View::SetLayoutManager(std::make_unique<views::FlexLayout>());
  flex_layout->SetOrientation(views::LayoutOrientation::kHorizontal);

  auto ink_drop_host_unique = std::make_unique<views::InkDropHost>(this);
  views::InkDropHost* ink_drop_host = ink_drop_host_unique.get();
  views::InkDrop::Install(this, std::move(ink_drop_host_unique));
  views::InstallRectHighlightPathGenerator(this);
  ink_drop_host->SetMode(views::InkDropHost::InkDropMode::ON);
  ink_drop_host->SetBaseColorId(ui::kColorSysStateHoverOnSubtle);
  ink_drop_host->SetHighlightOpacity(1.0f);
  ink_drop_host->GetInkDrop()->SetHoverHighlightFadeDuration(base::TimeDelta());

  content::WebContents* const web_contents = tab.GetWebContents();
  CHECK(web_contents);

  TabUIHelper* const tab_ui_helper = TabUIHelper::FromWebContents(web_contents);
  CHECK(tab_ui_helper);

  views::ImageView* const favicon = AddChildView(
      std::make_unique<views::ImageView>(tab_ui_helper->GetFavicon()));

  favicon->SetBackground(views::CreateThemedRoundedRectBackground(
      ui::kColorSysNeutralContainer, kFaviconCornerRadius));
  favicon->SetBorder(views::CreateThemedRoundedRectBorder(
      kFaviconBorderThickness, kFaviconCornerRadius,
      ui::kColorSysNeutralContainer));

  // Use the dialog bubble's insets to give the favicon and and close button
  // some space away from the edge of row view.
  views::LayoutProvider* const layout_provider = views::LayoutProvider::Get();
  CHECK(layout_provider);
  const gfx::Insets side_insets =
      layout_provider->GetInsetsMetric(views::InsetsMetric::INSETS_DIALOG);
  favicon->SetProperty(
      views::kMarginsKey,
      gfx::Insets::TLBR(kFaviconVerticalMargin, side_insets.left(),
                        kFaviconVerticalMargin,
                        ChromeLayoutProvider::Get()->GetDistanceMetric(
                            views::DISTANCE_RELATED_CONTROL_HORIZONTAL)));

  AddChildView(CreateTextView(tab_ui_helper->GetTitle(),
                              web_contents->GetLastCommittedURL()));

  std::unique_ptr<views::ImageButton> close_button =
      views::CreateVectorImageButtonWithNativeTheme(
          base::BindOnce(std::move(close_button_callback), this),
          views::kIcCloseIcon);

  // The close button should not be visible by default and should show up when
  // the user's mouse is over TabListRowView.
  close_button->SetVisible(false);
  close_button->SetProperty(
      views::kFlexBehaviorKey,
      views::FlexSpecification().WithAlignment(views::LayoutAlignment::kEnd));
  close_button->SetProperty(views::kMarginsKey,
                            gfx::Insets::TLBR(0, 0, 0, side_insets.right()));
  views::InstallCircleHighlightPathGenerator(close_button.get());
  close_button->SetAccessibleName(l10n_util::GetStringUTF16(IDS_ACCNAME_CLOSE));
  close_button_ = AddChildView(std::move(close_button));

  inkdrop_container_->SetProperty(views::kViewIgnoredByLayoutKey, true);
  SetNotifyEnterExitOnChild(true);
}

TabListRowView::~TabListRowView() = default;

std::u16string TabListRowView::GetTitleTextForTesting() {
  return title_->GetText();
}

std::u16string TabListRowView::GetDomainTextForTesting() {
  return domain_->GetText();
}

views::ImageButton* TabListRowView::GetCloseButtonForTesting() {
  return close_button_;
}

std::unique_ptr<views::View> TabListRowView::CreateTextView(
    std::u16string title,
    GURL domain) {
  auto text_view = std::make_unique<views::View>();
  views::FlexLayout* const flex_layout =
      text_view->SetLayoutManager(std::make_unique<views::FlexLayout>());
  flex_layout->SetOrientation(views::LayoutOrientation::kVertical);
  flex_layout->SetMainAxisAlignment(views::LayoutAlignment::kCenter);

  title_ = text_view->AddChildView(
      CreateLabel(title, views::style::STYLE_BODY_4_MEDIUM));
  domain_ = text_view->AddChildView(
      CreateLabel(url_formatter::FormatUrl(
                      domain,
                      url_formatter::kFormatUrlOmitDefaults |
                          url_formatter::kFormatUrlOmitHTTPS |
                          url_formatter::kFormatUrlOmitTrivialSubdomains |
                          url_formatter::kFormatUrlTrimAfterHost,
                      base::UnescapeRule::NORMAL, nullptr, nullptr, nullptr),
                  views::style::STYLE_BODY_5));
  return text_view;
}

void TabListRowView::OnMouseEntered(const ui::MouseEvent& event) {
  views::View::OnMouseEntered(event);
  // Show the highlight and "X" button when there is more than one item in the
  // tab list.
  const bool should_show_highlight =
      tab_list_model_->page_contexts().size() > 1;
  if (!should_show_highlight) {
    views::InkDrop::Get(this)->SetMode(views::InkDropHost::InkDropMode::OFF);
  }
  close_button_->SetVisible(should_show_highlight);
}

void TabListRowView::OnMouseExited(const ui::MouseEvent& event) {
  View::OnMouseExited(event);
  close_button_->SetVisible(false);
}

void TabListRowView::AddLayerToRegion(ui::Layer* layer,
                                      views::LayerRegion region) {
  inkdrop_container_->AddLayerToRegion(layer, region);
}

void TabListRowView::RemoveLayerFromRegions(ui::Layer* layer) {
  inkdrop_container_->RemoveLayerFromRegions(layer);
}

BEGIN_METADATA(TabListRowView)
END_METADATA
