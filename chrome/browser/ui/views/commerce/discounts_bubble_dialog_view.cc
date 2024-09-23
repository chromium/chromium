// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/commerce/discounts_bubble_dialog_view.h"

#include "base/functional/callback_forward.h"
#include "base/i18n/time_formatting.h"
#include "chrome/browser/ui/commerce/commerce_ui_tab_helper.h"
#include "chrome/browser/ui/tabs/public/tab_features.h"
#include "chrome/browser/ui/tabs/public/tab_interface.h"
#include "chrome/browser/ui/views/accessibility/theme_tracking_non_accessible_image_view.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/commerce/discounts_coupon_code_label_view.h"
#include "chrome/browser/ui/views/controls/page_switcher_view.h"
#include "chrome/browser/ui/views/controls/subpage_view.h"
#include "chrome/grit/theme_resources.h"
#include "components/commerce/core/commerce_types.h"
#include "components/commerce/core/metrics/discounts_metric_collector.h"
#include "components/strings/grit/components_strings.h"
#include "components/url_formatter/elide_url.h"
#include "content/public/browser/web_contents.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/view_tracker.h"
#include "url/gurl.h"

DEFINE_ELEMENT_IDENTIFIER_VALUE(kDiscountsBubbleDialogId);
DEFINE_ELEMENT_IDENTIFIER_VALUE(kDiscountsBubbleMainPageId);
DEFINE_ELEMENT_IDENTIFIER_VALUE(kDiscountsBubbleTermsAndConditionLabelId);
DEFINE_ELEMENT_IDENTIFIER_VALUE(kDiscountsBubbleTermsAndConditionPageId);

// DiscountsBubbleDialogView
DiscountsBubbleDialogView::DiscountsBubbleDialogView(
    View* anchor_view,
    content::WebContents* web_contents,
    const commerce::DiscountInfo& discount_info)
    : LocationBarBubbleDelegateView(anchor_view, web_contents, true),
      discount_info_(discount_info),
      ukm_source_id_(
          web_contents->GetPrimaryMainFrame()->GetPageUkmSourceId()) {
  SetProperty(views::kElementIdentifierKey, kDiscountsBubbleDialogId);

  SetShowCloseButton(true);
  SetCloseCallback(base::BindOnce(&DiscountsBubbleDialogView::OnDialogClosing,
                                  weak_factory_.GetWeakPtr()));

  set_fixed_width(views::LayoutProvider::Get()->GetDistanceMetric(
      views::DISTANCE_BUBBLE_PREFERRED_WIDTH));
  SetLayoutManager(std::make_unique<views::FillLayout>());
  auto dialog_insets =
      ChromeLayoutProvider::Get()->GetDialogInsetsForContentType(
          views::DialogContentType::kControl, views::DialogContentType::kText);
  set_margins(
      gfx::Insets::TLBR(dialog_insets.top(), 0, dialog_insets.bottom(), 0));
  SetButtons(static_cast<int>(ui::mojom::DialogButton::kNone));

  GURL url = web_contents->GetLastCommittedURL();
  std::string seller_domain = url.spec();

  page_container_ = AddChildView(std::make_unique<PageSwitcherView>(
      CreateMainPageContent(discount_info, seller_domain)));
}

DiscountsBubbleDialogView::~DiscountsBubbleDialogView() = default;

void DiscountsBubbleDialogView::AddedToWidget() {
  GetBubbleFrameView()->SetHeaderView(CreateMainPageHeaderView());
  GetBubbleFrameView()->SetTitleView(CreateMainPageTitleView(discount_info_));
}

std::unique_ptr<views::View>
DiscountsBubbleDialogView::CreateMainPageHeaderView() {
  ui::ResourceBundle& bundle = ui::ResourceBundle::GetSharedInstance();

  return std::make_unique<ThemeTrackingNonAccessibleImageView>(
      *bundle.GetImageSkiaNamed(IDR_DISCOUNTS_BUBBLE_HEADER_LIGHT),
      *bundle.GetImageSkiaNamed(IDR_DISCOUNTS_BUBBLE_HEADER_DARK),
      base::BindRepeating(&views::BubbleDialogDelegate::GetBackgroundColor,
                          base::Unretained(this)));
}

std::unique_ptr<views::View> DiscountsBubbleDialogView::CreateMainPageTitleView(
    const commerce::DiscountInfo& discount_info) {
  auto title_view = std::make_unique<views::Label>(
      base::ASCIIToUTF16(discount_info_.description_detail),
      views::style::CONTEXT_DIALOG_TITLE);
  title_view->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  title_view->SetMultiLine(true);
  return title_view;
}

std::unique_ptr<views::View> DiscountsBubbleDialogView::CreateMainPageContent(
    const commerce::DiscountInfo& discount_info,
    const std::string& seller_domain) {
  auto main_page_view = std::make_unique<views::View>();

  main_page_view->SetProperty(views::kElementIdentifierKey,
                              kDiscountsBubbleMainPageId);
  gfx::Insets dialog_insets =
      ChromeLayoutProvider::Get()->GetDialogInsetsForContentType(
          views::DialogContentType::kControl, views::DialogContentType::kText);

  auto* layout =
      main_page_view->SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kVertical,
          gfx::Insets::TLBR(0, dialog_insets.left(), 0, dialog_insets.right()),
          ChromeLayoutProvider::Get()->GetDistanceMetric(
              views::DISTANCE_UNRELATED_CONTROL_VERTICAL)));
  layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kStart);

  // coupon code
  main_page_view->AddChildView(std::make_unique<DiscountsCouponCodeLabelView>(
      base::ASCIIToUTF16(discount_info.discount_code.value()),
      base::BindRepeating(&DiscountsBubbleDialogView::CopyButtonClicked,
                          weak_factory_.GetWeakPtr())));

  // additional info with expiration date, and terms and conditions
  auto* additional_info_label = main_page_view->AddChildView(
      views::Builder<views::StyledLabel>()
          .SetDefaultTextStyle(views::style::STYLE_SECONDARY)
          .SetTextContext(views::style::CONTEXT_DIALOG_BODY_TEXT)
          .SetHorizontalAlignment(gfx::ALIGN_LEFT)
          .Build());

  auto additional_info_text = l10n_util::GetStringFUTF16(
      IDS_DISCOUNT_USE_THIS_CODE_AT_CHECKOUT_WITH_EXPIRATION_DATE,
      TimeFormatShortDate(base::Time::FromSecondsSinceUnixEpoch(
          discount_info.expiry_time_sec)));

  if (discount_info.terms_and_conditions.has_value() &&
      !discount_info.terms_and_conditions.value().empty()) {
    std::vector<size_t> offsets;
    additional_info_text = l10n_util::GetStringFUTF16(
        IDS_TWO_STRINGS_CONNECTOR_WITH_SPACE, additional_info_text,
        l10n_util::GetStringUTF16(IDS_SEE_SELLER_TERMS_AND_CONDITIONS),
        &offsets);
    additional_info_label->SetText(additional_info_text);
    size_t terms_and_conditions_offset = offsets[1];
    base::RepeatingCallback<void()> callback = base::BindRepeating(
        &DiscountsBubbleDialogView::OpenTermsAndConditionsPage,
        weak_factory_.GetWeakPtr(), discount_info, seller_domain);
    views::StyledLabel::RangeStyleInfo terms_and_conditions_style_info =
        views::StyledLabel::RangeStyleInfo::CreateForLink(std::move(callback));
    additional_info_label->AddStyleRange(
        gfx::Range(terms_and_conditions_offset, additional_info_text.length()),
        terms_and_conditions_style_info);
    additional_info_label->SetProperty(
        views::kElementIdentifierKey, kDiscountsBubbleTermsAndConditionLabelId);
  } else {
    additional_info_label->SetText(additional_info_text);
  }

  return main_page_view;
}

void DiscountsBubbleDialogView::OpenMainPage(
    commerce::DiscountInfo discount_info,
    std::string seller_domain) {
  GetBubbleFrameView()->SetHeaderView(CreateMainPageHeaderView());
  GetBubbleFrameView()->SetTitleView(CreateMainPageTitleView(discount_info));
  page_container_->SwitchToPage(
      CreateMainPageContent(discount_info, seller_domain));
  GetBubbleFrameView()->SetFootnoteView(nullptr);
}

void DiscountsBubbleDialogView::OpenTermsAndConditionsPage(
    commerce::DiscountInfo discount_info,
    std::string seller_domain) {
  auto bubble_width = views::LayoutProvider::Get()->GetDistanceMetric(
      views::DISTANCE_BUBBLE_PREFERRED_WIDTH);

  gfx::FontList font_list = views::TypographyProvider::Get().GetFont(
      views::style::CONTEXT_LABEL, views::style::STYLE_PRIMARY);

  auto footer_message = l10n_util::GetStringFUTF16(
      IDS_SELLER_TERMS_AND_CONDITIONS_DIALOG_FOOTER,
      url_formatter::ElideHost(GURL(seller_domain), font_list, bubble_width));

  page_container_->SwitchToPage(
      views::Builder<SubpageView>(
          std::make_unique<SubpageView>(
              base::BindRepeating(&DiscountsBubbleDialogView::OpenMainPage,
                                  weak_factory_.GetWeakPtr(), discount_info,
                                  seller_domain),
              GetBubbleFrameView()))
          .SetProperty(views::kElementIdentifierKey,
                       kDiscountsBubbleTermsAndConditionPageId)
          .SetTitle(l10n_util::GetStringUTF16(
              IDS_SELLER_TERMS_AND_CONDITIONS_DIALOG_TITLE))
          .SetContentView(
              views::Builder<views::Label>()
                  .SetText(base::ASCIIToUTF16(
                      discount_info.terms_and_conditions.value()))
                  .SetMultiLine(true)
                  .SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT)
                  .Build())
          .SetHeaderView(nullptr)
          .SetFootnoteView(
              views::Builder<views::Label>()
                  .SetText(footer_message)
                  .SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT)
                  .SetMultiLine(true)
                  .SetAllowCharacterBreak(true)
                  .Build())
          .Build());
}

void DiscountsBubbleDialogView::CopyButtonClicked() {
  commerce::metrics::DiscountsMetricCollector::
      RecordDiscountsBubbleCopyButtonClicked(ukm_source_id_);

  auto* tab_helper = tabs::TabInterface::GetFromContents(web_contents())
                         ->GetTabFeatures()
                         ->commerce_ui_tab_helper();

  if (!tab_helper) {
    return;
  }

  tab_helper->OnDiscountsCouponCodeCopied();
}

void DiscountsBubbleDialogView::OnDialogClosing() {
  auto* tab_helper = tabs::TabInterface::GetFromContents(web_contents())
                         ->GetTabFeatures()
                         ->commerce_ui_tab_helper();

  if (!tab_helper) {
    return;
  }

  commerce::metrics::DiscountsMetricCollector::
      DiscountsBubbleCopyStatusOnBubbleClosed(
          tab_helper->IsDiscountsCouponCodeCopied(),
          tab_helper->GetDiscounts());
}

BEGIN_METADATA(DiscountsBubbleDialogView)
END_METADATA

// DiscountsBubbleCoordinator
DiscountsBubbleCoordinator::DiscountsBubbleCoordinator(views::View* anchor_view)
    : anchor_view_(anchor_view) {}

DiscountsBubbleCoordinator::~DiscountsBubbleCoordinator() = default;

// WidgetObserver:
void DiscountsBubbleCoordinator::OnWidgetDestroying(views::Widget* widget) {
  CHECK(bubble_widget_observation_.IsObservingSource(widget));
  bubble_widget_observation_.Reset();

  std::move(on_dialog_closing_callback_).Run();
}

void DiscountsBubbleCoordinator::Show(
    content::WebContents* web_contents,
    const commerce::DiscountInfo& discount_info,
    base::OnceClosure on_dialog_closing_callback) {
  CHECK(!IsShowing());

  on_dialog_closing_callback_ = std::move(on_dialog_closing_callback);

  auto bubble = std::make_unique<DiscountsBubbleDialogView>(
      anchor_view_, web_contents, discount_info);
  tracker_.SetView(bubble.get());
  auto* widget = DiscountsBubbleDialogView::CreateBubble(std::move(bubble));
  bubble_widget_observation_.Observe(widget);
  widget->Show();
}

void DiscountsBubbleCoordinator::Hide() {
  if (IsShowing()) {
    tracker_.view()->GetWidget()->Close();
  }
  tracker_.SetView(nullptr);
}

DiscountsBubbleDialogView* DiscountsBubbleCoordinator::GetBubble() const {
  return tracker_.view() ? views::AsViewClass<DiscountsBubbleDialogView>(
                               const_cast<views::View*>(tracker_.view()))
                         : nullptr;
}

bool DiscountsBubbleCoordinator::IsShowing() {
  return tracker_.view() != nullptr;
}
