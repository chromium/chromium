// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/page_info/safety_tip_page_info_bubble_view.h"

#include "base/functional/bind.h"
#include "chrome/browser/lookalikes/safety_tip_ui_helper.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/views/accessibility/theme_tracking_non_accessible_image_view.h"
#include "chrome/browser/ui/views/bubble_anchor_util_views.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/page_info/page_info_view_factory.h"
#include "chrome/grit/theme_resources.h"
#include "components/security_state/core/security_state.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/navigation_handle.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/color_utils.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/style/typography.h"
#include "ui/views/widget/widget.h"
#include "url/gurl.h"

SafetyTipPageInfoBubbleView::SafetyTipPageInfoBubbleView(
    views::View* anchor_view,
    const gfx::Rect& anchor_rect,
    gfx::NativeView parent_window,
    content::WebContents* web_contents,
    security_state::SafetyTipStatus safety_tip_status,
    const GURL& suggested_url,
    base::OnceCallback<void(SafetyTipInteraction)> close_callback)
    : PageInfoBubbleViewBase(anchor_view,
                             anchor_rect,
                             parent_window,
                             PageInfoBubbleViewBase::BUBBLE_SAFETY_TIP,
                             web_contents),
      safety_tip_status_(safety_tip_status),
      suggested_url_(suggested_url),
      close_callback_(std::move(close_callback)) {
  // Keep the bubble open until explicitly closed (or we navigate away, a tab is
  // created over it, etc).
  set_close_on_deactivate(false);

  const std::u16string title_text =
      GetSafetyTipTitle(safety_tip_status, suggested_url);
  SetTitle(title_text);

  views::BubbleDialogDelegateView::CreateBubble(this);

  // Replace the original title view with our formatted title.
  views::Label* original_title =
      static_cast<views::Label*>(GetBubbleFrameView()->title());
  views::StyledLabel::RangeStyleInfo name_style;
  const auto kSizeDeltaInPixels = 3;
  name_style.custom_font = original_title->GetDefaultFontList().Derive(
      kSizeDeltaInPixels, gfx::Font::FontStyle::NORMAL,
      gfx::Font::Weight::BOLD);
  views::StyledLabel::RangeStyleInfo base_style;
  base_style.custom_font = original_title->GetDefaultFontList().Derive(
      kSizeDeltaInPixels, gfx::Font::FontStyle::NORMAL,
      gfx::Font::Weight::NORMAL);

  auto new_title = std::make_unique<views::StyledLabel>();
  new_title->SetText(title_text);
  new_title->AddStyleRange(gfx::Range(0, title_text.length()), name_style);
  GetBubbleFrameView()->SetTitleView(std::move(new_title));
  set_margins(gfx::Insets::TLBR(0, 0, margins().bottom(), 0));

  // Configure layout.
  ChromeLayoutProvider* layout_provider = ChromeLayoutProvider::Get();
  gfx::Insets insets = layout_provider->GetDialogInsetsForContentType(
      views::DialogContentType::kText, views::DialogContentType::kText);
  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical,
      gfx::Insets::TLBR(insets.top(), insets.left(), 0, insets.right()),
      insets.bottom()));

  // Configure header view.
  auto& bundle = ui::ResourceBundle::GetSharedInstance();
  auto header_view = std::make_unique<ThemeTrackingNonAccessibleImageView>(
      *bundle.GetImageSkiaNamed(IDR_SAFETY_TIP_ILLUSTRATION_LIGHT),
      *bundle.GetImageSkiaNamed(IDR_SAFETY_TIP_ILLUSTRATION_DARK),
      base::BindRepeating(&views::BubbleDialogDelegate::GetBackgroundColor,
                          base::Unretained(this)));
  GetBubbleFrameView()->SetHeaderView(std::move(header_view));

  // Add text description.
  auto* text_label = AddChildView(std::make_unique<views::Label>(
      GetSafetyTipDescription(safety_tip_status, suggested_url_)));
  text_label->SetMultiLine(true);
  text_label->SetLineHeight(20);
  text_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  text_label->SizeToFit(layout_provider->GetDistanceMetric(
                            views::DISTANCE_BUBBLE_PREFERRED_WIDTH) -
                        insets.left() - insets.right());

  auto* button_view = AddChildView(std::make_unique<views::BoxLayoutView>());
  button_view->SetCrossAxisAlignment(
      views::BoxLayout::CrossAxisAlignment::kCenter);

  // Learn more link.
  info_link_ = button_view->AddChildView(std::make_unique<views::Link>(
      l10n_util::GetStringUTF16(IDS_PAGE_INFO_SAFETY_TIP_MORE_INFO_LINK)));
  info_link_->SetCallback(base::BindRepeating(
      &SafetyTipPageInfoBubbleView::OpenHelpCenter, base::Unretained(this)));

  auto* flex_view = button_view->AddChildView(std::make_unique<views::View>());
  button_view->SetFlexForView(flex_view, 1);

  // Leave site button.
  leave_button_ =
      button_view->AddChildView(std::make_unique<views::MdTextButton>(
          base::BindRepeating(
              [](SafetyTipPageInfoBubbleView* view) {
                view->ExecuteLeaveCommand();
              },
              this),
          l10n_util::GetStringUTF16(
              GetSafetyTipLeaveButtonId(safety_tip_status))));
  leave_button_->SetStyle(ui::ButtonStyle::kProminent);
  leave_button_->SetID(
      PageInfoViewFactory::VIEW_ID_PAGE_INFO_BUTTON_LEAVE_SITE);
}

SafetyTipPageInfoBubbleView::~SafetyTipPageInfoBubbleView() {}

void SafetyTipPageInfoBubbleView::OnWidgetDestroying(views::Widget* widget) {
  PageInfoBubbleViewBase::OnWidgetDestroying(widget);

  switch (widget->closed_reason()) {
    case views::Widget::ClosedReason::kUnspecified:
      // Do not modify action_taken_.  This may correspond to the
      // WebContentsObserver functions below, in which case a more explicit
      // action_taken_ is set. Otherwise, keep default of kNoAction.
      break;
    case views::Widget::ClosedReason::kLostFocus:
      // We require that the user explicitly interact with the bubble, so do
      // nothing in these cases.
      break;
    case views::Widget::ClosedReason::kAcceptButtonClicked:
      // If they've left the site, we can still ignore the result; if they
      // stumble there again, we should warn again.
      break;
    case views::Widget::ClosedReason::kEscKeyPressed:
      action_taken_ = SafetyTipInteraction::kDismissWithEsc;
      break;
    case views::Widget::ClosedReason::kCloseButtonClicked:
      action_taken_ = SafetyTipInteraction::kDismissWithClose;
      break;
    case views::Widget::ClosedReason::kCancelButtonClicked:
      // I don't know why, but ESC sometimes generates kCancelButtonClicked.
      action_taken_ = SafetyTipInteraction::kDismissWithEsc;
      break;
  }
  std::move(close_callback_).Run(action_taken_);
}

void SafetyTipPageInfoBubbleView::ExecuteLeaveCommand() {
  action_taken_ = SafetyTipInteraction::kLeaveSite;
  LeaveSiteFromSafetyTip(
      web_contents(),
      safety_tip_status_ == security_state::SafetyTipStatus::kLookalike
          ? suggested_url_
          : GURL());
}

void SafetyTipPageInfoBubbleView::OpenHelpCenter() {
  action_taken_ = SafetyTipInteraction::kLearnMore;
  OpenHelpCenterFromSafetyTip(web_contents());
}

void SafetyTipPageInfoBubbleView::RenderFrameDeleted(
    content::RenderFrameHost* render_frame_host) {
  if (!render_frame_host->IsInPrimaryMainFrame()) {
    return;
  }

  if (action_taken_ == SafetyTipInteraction::kNoAction) {
    action_taken_ = SafetyTipInteraction::kCloseTab;
  }

  // There's no great ClosedReason for this, so we use kUnspecified to signal
  // that a more specific action_taken_ may have already been set.
  GetWidget()->CloseWithReason(views::Widget::ClosedReason::kUnspecified);
}

void SafetyTipPageInfoBubbleView::OnVisibilityChanged(
    content::Visibility visibility) {
  if (visibility != content::Visibility::HIDDEN) {
    return;
  }

  if (action_taken_ == SafetyTipInteraction::kNoAction) {
    action_taken_ = SafetyTipInteraction::kSwitchTab;
  }

  // There's no great ClosedReason for this, so we use kUnspecified to signal
  // that a more specific action_taken_ may have already been set.
  GetWidget()->CloseWithReason(views::Widget::ClosedReason::kUnspecified);
}

void SafetyTipPageInfoBubbleView::PrimaryPageChanged(content::Page& page) {
  if (action_taken_ == SafetyTipInteraction::kNoAction) {
    action_taken_ = SafetyTipInteraction::kChangePrimaryPage;
  }

  // There's no great ClosedReason for this, so we use kUnspecified to signal
  // that a more specific action_taken_ may have already been set.
  GetWidget()->CloseWithReason(views::Widget::ClosedReason::kUnspecified);
}

void SafetyTipPageInfoBubbleView::DidChangeVisibleSecurityState() {
  // Do nothing. (Base class closes the bubble.)
}

void ShowSafetyTipDialog(
    content::WebContents* web_contents,
    security_state::SafetyTipStatus safety_tip_status,
    const GURL& suggested_url,
    base::OnceCallback<void(SafetyTipInteraction)> close_callback) {
  Browser* browser = chrome::FindBrowserWithTab(web_contents);
  if (!browser)
    return;

  bubble_anchor_util::AnchorConfiguration configuration =
      bubble_anchor_util::GetPageInfoAnchorConfiguration(
          browser, bubble_anchor_util::kLocationBar);
  gfx::Rect anchor_rect =
      configuration.anchor_view
          ? gfx::Rect()
          : bubble_anchor_util::GetPageInfoAnchorRect(browser);
  gfx::NativeWindow parent_window = browser->window()->GetNativeWindow();
  gfx::NativeView parent_view = platform_util::GetViewForWindow(parent_window);

  views::BubbleDialogDelegateView* bubble = new SafetyTipPageInfoBubbleView(
      configuration.anchor_view, anchor_rect, parent_view, web_contents,
      safety_tip_status, suggested_url, std::move(close_callback));

  bubble->SetHighlightedButton(configuration.highlighted_button);
  bubble->SetArrow(configuration.bubble_arrow);
  bubble->GetWidget()->Show();
}

PageInfoBubbleViewBase* CreateSafetyTipBubbleForTesting(
    gfx::NativeView parent_view,
    content::WebContents* web_contents,
    security_state::SafetyTipStatus safety_tip_status,
    const GURL& suggested_url,
    base::OnceCallback<void(SafetyTipInteraction)> close_callback) {
  return new SafetyTipPageInfoBubbleView(
      nullptr, gfx::Rect(), parent_view, web_contents, safety_tip_status,
      suggested_url, std::move(close_callback));
}
