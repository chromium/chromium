// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/page_info/accuracy_tip_bubble_view.h"

#include "base/bind.h"
#include "base/callback.h"
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
#include "components/accuracy_tips/accuracy_tip_status.h"
#include "components/accuracy_tips/accuracy_tip_ui.h"
#include "components/strings/grit/components_strings.h"
#include "components/vector_icons/vector_icons.h"
#include "content/public/browser/navigation_handle.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/image_model.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/layout/flex_layout_types.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/layout/layout_types.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/widget/widget.h"
#include "url/gurl.h"

namespace {

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
  icon_view->SetImage(ui::ImageModel::FromVectorIcon(
      icon, ui::NativeTheme::kColorId_DefaultIconColor, kVectorIconSize));
  icon_view->SetProperty(views::kMarginsKey, gfx::Insets(0, 0, 0, icon_margin));
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
    base::OnceCallback<void(AccuracyTipUI::Interaction)> close_callback)
    : PageInfoBubbleViewBase(anchor_view,
                             anchor_rect,
                             parent_window,
                             PageInfoBubbleViewBase::BUBBLE_ACCURACY_TIP,
                             web_contents),
      close_callback_(std::move(close_callback)) {
  DCHECK(status == accuracy_tips::AccuracyTipStatus::kShowAccuracyTip);

  views::BubbleDialogDelegateView::CreateBubble(this);

  SetTitle(l10n_util::GetStringUTF16(IDS_PAGE_INFO_ACCURACY_TIP_TITLE));

  // Configure buttons.
  SetButtons(ui::DIALOG_BUTTON_OK | ui::DIALOG_BUTTON_CANCEL);
  SetButtonLabel(
      ui::DIALOG_BUTTON_OK,
      l10n_util::GetStringUTF16(IDS_PAGE_INFO_ACCURACY_TIP_LEARN_MORE_BUTTON));
  SetButtonLabel(
      ui::DIALOG_BUTTON_CANCEL,
      l10n_util::GetStringUTF16(IDS_PAGE_INFO_ACCURACY_TIP_IGNORE_BUTTON));
  SetAcceptCallback(base::BindRepeating(&AccuracyTipBubbleView::OpenHelpCenter,
                                        base::Unretained(this)));

  // Configure header view.
  auto& bundle = ui::ResourceBundle::GetSharedInstance();
  auto header_view = std::make_unique<ThemeTrackingNonAccessibleImageView>(
      *bundle.GetImageSkiaNamed(IDR_SAFETY_TIP_ILLUSTRATION_LIGHT),
      *bundle.GetImageSkiaNamed(IDR_SAFETY_TIP_ILLUSTRATION_DARK),
      base::BindRepeating(&views::BubbleFrameView::GetBackgroundColor,
                          base::Unretained(GetBubbleFrameView())));
  set_fixed_width(header_view->GetPreferredSize().width());
  GetBubbleFrameView()->SetHeaderView(std::move(header_view));

  // Configure main content.
  auto* provider = ChromeLayoutProvider::Get();
  int vertical_margin =
      provider->GetDistanceMetric(views::DISTANCE_RELATED_CONTROL_VERTICAL);
  SetLayoutManager(std::make_unique<views::FlexLayout>())
      ->SetOrientation(views::LayoutOrientation::kVertical)
      .SetCollapseMargins(true)
      .SetDefault(views::kMarginsKey, gfx::Insets(vertical_margin, 0))
      .SetDefault(
          views::kFlexBehaviorKey,
          views::FlexSpecification(views::MinimumFlexSizeRule::kScaleToZero,
                                   views::MaximumFlexSizeRule::kUnbounded,
                                   /*adjust_height_for_width =*/true)
              .WithWeight(1));

  // TODO(crbug.com/1210891): Replace placeholder strings and icons.
  AddChildView(CreateRow(u"Verify the organizations's authority on the topic",
                         vector_icons::kCertificateIcon));
  AddChildView(
      CreateRow(u"Check the source and evidence", vector_icons::kSettingsIcon));

  Layout();
  SizeToContents();
}

AccuracyTipBubbleView::~AccuracyTipBubbleView() = default;

void AccuracyTipBubbleView::OnWidgetDestroying(views::Widget* widget) {
  PageInfoBubbleViewBase::OnWidgetDestroying(widget);

  switch (widget->closed_reason()) {
    case views::Widget::ClosedReason::kUnspecified:
      // Do not modify action_taken_.  This may correspond to the
      // WebContentsObserver functions below, in which case a more explicit
      // action_taken_ may be set. Otherwise, keep default of kNoAction.
      break;
    case views::Widget::ClosedReason::kAcceptButtonClicked:
      action_taken_ = AccuracyTipUI::Interaction::kLearnMorePressed;
      break;
    case views::Widget::ClosedReason::kCancelButtonClicked:
      action_taken_ = AccuracyTipUI::Interaction::kIgnorePressed;
      break;
    case views::Widget::ClosedReason::kLostFocus:
      action_taken_ = AccuracyTipUI::Interaction::kLostFocus;
      break;
    case views::Widget::ClosedReason::kEscKeyPressed:
    case views::Widget::ClosedReason::kCloseButtonClicked:
      action_taken_ = AccuracyTipUI::Interaction::kClosed;
      break;
  }
  std::move(close_callback_).Run(action_taken_);
}

void AccuracyTipBubbleView::OpenHelpCenter() {
  action_taken_ = AccuracyTipUI::Interaction::kLearnMorePressed;
  // TODO(crbug.com/1210891): Add link to the right info page.
  web_contents()->OpenURL(content::OpenURLParams(
      GURL(chrome::kSafetyTipHelpCenterURL), content::Referrer(),
      WindowOpenDisposition::NEW_FOREGROUND_TAB, ui::PAGE_TRANSITION_LINK,
      false /*is_renderer_initiated*/));
}

void AccuracyTipBubbleView::DidStartNavigation(
    content::NavigationHandle* handle) {
  // TODO(https://crbug.com/1218946): With MPArch there may be multiple main
  // frames. This caller was converted automatically to the primary main frame
  // to preserve its semantics. Follow up to confirm correctness.
  if (!handle->IsInPrimaryMainFrame() || handle->IsSameDocument()) {
    return;
  }
  GetWidget()->Close();
}

void AccuracyTipBubbleView::DidChangeVisibleSecurityState() {
  // Do nothing. (Base class closes the bubble.)
}

// Implementation for c/b/ui/chrome_accuracy_tip_ui.h
void ShowAccuracyTipDialog(
    content::WebContents* web_contents,
    accuracy_tips::AccuracyTipStatus status,
    base::OnceCallback<void(accuracy_tips::AccuracyTipUI::Interaction)>
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
      std::move(close_callback));

  bubble->SetHighlightedButton(configuration.highlighted_button);
  bubble->SetArrow(configuration.bubble_arrow);
  bubble->GetWidget()->Show();
}
