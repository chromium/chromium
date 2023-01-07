// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/page_info/accuracy_tip_bubble_view.h"

#include <memory>

#include "base/bind.h"
#include "base/callback.h"
#include "base/notreached.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/page_info/chrome_accuracy_tip_ui.h"
#include "chrome/browser/ui/views/accessibility/non_accessible_image_view.h"
#include "chrome/browser/ui/views/accessibility/theme_tracking_non_accessible_image_view.h"
#include "chrome/browser/ui/views/bubble_anchor_util_views.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/theme_resources.h"
#include "components/accuracy_tips/accuracy_tip_interaction.h"
#include "components/accuracy_tips/accuracy_tip_status.h"
#include "components/accuracy_tips/features.h"
#include "components/strings/grit/components_strings.h"
#include "components/vector_icons/vector_icons.h"
#include "content/public/browser/navigation_handle.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/image_model.h"
#include "ui/color/color_id.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/layout/flex_layout_types.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/layout/layout_types.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/widget/widget.h"
#include "url/gurl.h"

namespace {

using ClosedReason = views::Widget::ClosedReason;

// The icon size is actually 16, but the vector icons being used generally all
// have additional internal padding. Account for this difference by asking for
// the vectors in 18x18dip sizes.
constexpr int kVectorIconSize = 18;

std::unique_ptr<views::View> CreateRow(const std::u16string& text,
                                       const gfx::VectorIcon& icon) {
  auto line = std::make_unique<views::View>();
  line->SetLayoutManager(std::make_unique<views::FlexLayout>())
      ->SetOrientation(views::LayoutOrientation::kHorizontal);

  auto* provider = ChromeLayoutProvider::Get();
  int icon_margin =
      provider->GetDistanceMetric(views::DISTANCE_RELATED_LABEL_HORIZONTAL);

  auto icon_view = std::make_unique<NonAccessibleImageView>();
  icon_view->SetImage(
      ui::ImageModel::FromVectorIcon(icon, ui::kColorIcon, kVectorIconSize));
  icon_view->SetProperty(views::kMarginsKey,
                         gfx::Insets::TLBR(0, 0, 0, icon_margin));
  icon_view->SetProperty(views::kCrossAxisAlignmentKey,
                         views::LayoutAlignment::kStart);
  line->AddChildView(std::move(icon_view));

  auto text_view = std::make_unique<views::Label>(
      text, views::style::CONTEXT_LABEL, views::style::STYLE_SECONDARY);
  text_view->SetMultiLine(true);
  text_view->SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT);
  text_view->SetProperty(
      views::kFlexBehaviorKey,
      views::FlexSpecification(views::MinimumFlexSizeRule::kScaleToZero,
                               views::MaximumFlexSizeRule::kUnbounded,
                               /*adjust_height_for_width =*/true)
          .WithWeight(1));
  line->AddChildView(std::move(text_view));

  return line;
}

}  // namespace

AccuracyTipBubbleView::AccuracyTipBubbleView(
    views::View* anchor_view,
    const gfx::Rect& anchor_rect,
    gfx::NativeView parent_window,
    content::WebContents* web_contents,
    accuracy_tips::AccuracyTipStatus status,
    bool show_opt_out,
    base::OnceCallback<void(AccuracyTipInteraction)> close_callback)
    : PageInfoBubbleViewBase(anchor_view,
                             anchor_rect,
                             parent_window,
                             PageInfoBubbleViewBase::BUBBLE_ACCURACY_TIP,
                             web_contents),
      close_callback_(std::move(close_callback)) {
  DCHECK(status == accuracy_tips::AccuracyTipStatus::kShowAccuracyTip);
  set_close_on_deactivate(false);

  SetTitle(l10n_util::GetStringUTF16(IDS_PAGE_INFO_ACCURACY_TIP_TITLE));

  // Configure buttons.
  SetButtons(ui::DIALOG_BUTTON_OK);
  SetButtonLabel(
      ui::DIALOG_BUTTON_OK,
      l10n_util::GetStringUTF16(IDS_PAGE_INFO_ACCURACY_TIP_LEARN_MORE_BUTTON));
  SetAcceptCallback(base::BindRepeating(&AccuracyTipBubbleView::OpenHelpCenter,
                                        base::Unretained(this)));

  SetExtraView(std::make_unique<views::MdTextButton>(
      base::BindRepeating(&AccuracyTipBubbleView::OnSecondaryButtonClicked,
                          base::Unretained(this),
                          show_opt_out ? AccuracyTipInteraction::kOptOut
                                       : AccuracyTipInteraction::kIgnore),
      l10n_util::GetStringUTF16(
          show_opt_out ? IDS_PAGE_INFO_ACCURACY_TIP_OPT_OUT_BUTTON
                       : IDS_PAGE_INFO_ACCURACY_TIP_IGNORE_BUTTON)));

  // The extra view doesn't seem to work if CreateBubble is already called and
  // SetHeaderView can only be called afterwards...
  views::BubbleDialogDelegateView::CreateBubble(this);

  // Configure header view.
  auto& bundle = ui::ResourceBundle::GetSharedInstance();
  auto header_view = std::make_unique<ThemeTrackingNonAccessibleImageView>(
      *bundle.GetImageSkiaNamed(IDR_ACCURACY_TIP_ILLUSTRATION_LIGHT),
      *bundle.GetImageSkiaNamed(IDR_ACCURACY_TIP_ILLUSTRATION_DARK),
      base::BindRepeating(&views::BubbleDialogDelegate::GetBackgroundColor,
                          base::Unretained(this)));
  set_fixed_width(header_view->GetPreferredSize().width());
  GetBubbleFrameView()->SetHeaderView(std::move(header_view));

  // Configure main content.
  auto* provider = ChromeLayoutProvider::Get();
  int vertical_margin =
      provider->GetDistanceMetric(views::DISTANCE_RELATED_CONTROL_VERTICAL);
  SetLayoutManager(std::make_unique<views::FlexLayout>())
      ->SetOrientation(views::LayoutOrientation::kVertical)
      .SetCollapseMargins(true)
      .SetDefault(views::kMarginsKey, gfx::Insets::VH(vertical_margin, 0))
      .SetDefault(
          views::kFlexBehaviorKey,
          views::FlexSpecification(views::MinimumFlexSizeRule::kScaleToZero,
                                   views::MaximumFlexSizeRule::kUnbounded,
                                   /*adjust_height_for_width =*/true)
              .WithWeight(1));

  AddChildView(CreateRow(
      l10n_util::GetStringUTF16(IDS_PAGE_INFO_ACCURACY_TIP_BODY_LINE_1),
      vector_icons::kGroupsIcon));

  AddChildView(CreateRow(
      l10n_util::GetStringUTF16(IDS_PAGE_INFO_ACCURACY_TIP_BODY_LINE_2),
      vector_icons::kTroubleshootIcon));

  AddChildView(CreateRow(
      l10n_util::GetStringUTF16(IDS_PAGE_INFO_ACCURACY_TIP_BODY_LINE_3),
      vector_icons::kFeedIcon));

  permissions::PermissionRequestManager* permission_request_manager =
      permissions::PermissionRequestManager::FromWebContents(web_contents);
  if (permission_request_manager) {
    permission_request_manager->AddObserver(this);
  }

  Layout();
  SizeToContents();
}

AccuracyTipBubbleView::~AccuracyTipBubbleView() {
  if (web_contents()) {
    permissions::PermissionRequestManager* permission_request_manager =
        permissions::PermissionRequestManager::FromWebContents(web_contents());
    if (permission_request_manager) {
      permission_request_manager->RemoveObserver(this);
    }
  }
}

void AccuracyTipBubbleView::OnWidgetDestroying(views::Widget* widget) {
  PageInfoBubbleViewBase::OnWidgetDestroying(widget);

  // There can either be an action already specified or a closed_reason.
  DCHECK(!(action_taken_ != AccuracyTipInteraction::kNoAction &&
           widget->closed_reason() != ClosedReason::kUnspecified));

  switch (widget->closed_reason()) {
    case ClosedReason::kUnspecified:
      // Do not modify action_taken_.  This may correspond to the
      // WebContentsObserver functions below, in which case a more explicit
      // action_taken_ may be set. Otherwise, keep default of kNoAction.
      break;
    case ClosedReason::kAcceptButtonClicked:
      action_taken_ = AccuracyTipInteraction::kLearnMore;
      break;
    case ClosedReason::kEscKeyPressed:
    case ClosedReason::kCloseButtonClicked:
    case ClosedReason::kCancelButtonClicked:
      action_taken_ = AccuracyTipInteraction::kClosed;
      break;
    case ClosedReason::kLostFocus:
      NOTREACHED();
      break;
  }
  std::move(close_callback_).Run(action_taken_);
}

void AccuracyTipBubbleView::OnPromptAdded() {
  // The page requested a permission that triggered a permission prompt.
  // Accuracy tips have lower priority and have to be closed.
  action_taken_ = AccuracyTipInteraction::kPermissionRequested;
  GetWidget()->Close();
}

void AccuracyTipBubbleView::OpenHelpCenter() {
  // TODO(crbug.com/1210891): Add link to the right info page.
  action_taken_ = AccuracyTipInteraction::kLearnMore;
  web_contents()->OpenURL(content::OpenURLParams(
      GURL(accuracy_tips::features::kLearnMoreUrl.Get().empty()
               ? chrome::kSafetyTipHelpCenterURL
               : accuracy_tips::features::kLearnMoreUrl.Get()),
      content::Referrer(), WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui::PAGE_TRANSITION_LINK, false /*is_renderer_initiated*/));
}

void AccuracyTipBubbleView::OnSecondaryButtonClicked(
    AccuracyTipInteraction action) {
  action_taken_ = action;
  GetWidget()->Close();
}

void AccuracyTipBubbleView::DidChangeVisibleSecurityState() {
  // Do nothing. (Base class closes the bubble.)
}

// Implementation for c/b/ui/chrome_accuracy_tip_interaction.h
void ShowAccuracyTipDialog(
    content::WebContents* web_contents,
    accuracy_tips::AccuracyTipStatus status,
    bool show_opt_out,
    base::OnceCallback<void(accuracy_tips::AccuracyTipInteraction)>
        close_callback) {
  Browser* browser = chrome::FindBrowserWithWebContents(web_contents);
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

  views::BubbleDialogDelegateView* bubble = new AccuracyTipBubbleView(
      configuration.anchor_view, anchor_rect, parent_view, web_contents, status,
      show_opt_out, std::move(close_callback));

  bubble->SetHighlightedButton(configuration.highlighted_button);
  bubble->SetArrow(configuration.bubble_arrow);
  bubble->GetWidget()->Show();
}
