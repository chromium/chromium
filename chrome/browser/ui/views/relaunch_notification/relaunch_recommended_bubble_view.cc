// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/relaunch_notification/relaunch_recommended_bubble_view.h"

#include <tuple>
#include <utility>

#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/toolbar/browser_app_menu_button.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/buildflags.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/image_model.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/text_constants.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/border.h"
#include "ui/views/bubble/bubble_border.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/style/typography.h"
#include "ui/views/widget/widget.h"

#if BUILDFLAG(IS_MAC)
#include "chrome/browser/platform_util.h"
#endif  // BUILDFLAG(IS_MAC)

// static
views::Widget* RelaunchRecommendedBubbleView::ShowBubble(
    Browser* browser,
    base::Time detection_time,
    base::RepeatingClosure on_accept) {
  DCHECK(browser);

  // Anchor the popup to the browser's app menu.
  auto* anchor_button = BrowserView::GetBrowserViewForBrowser(browser)
                            ->toolbar_button_provider()
                            ->GetAppMenuButton();
  auto* bubble_view = new RelaunchRecommendedBubbleView(
      anchor_button, detection_time, std::move(on_accept));
  bubble_view->SetArrow(views::BubbleBorder::TOP_RIGHT);

  views::Widget* bubble_widget =
      views::BubbleDialogDelegateView::CreateBubble(bubble_view);
  bubble_view->ShowForReason(AUTOMATIC);

  return bubble_widget;
}

RelaunchRecommendedBubbleView::~RelaunchRecommendedBubbleView() = default;

bool RelaunchRecommendedBubbleView::Accept() {
  base::RecordAction(base::UserMetricsAction("RelaunchRecommended_Accept"));

  on_accept_.Run();

  // Keep the bubble open in case shutdown is prevented for some reason so that
  // the user can try again if needed.
  return false;
}

std::u16string RelaunchRecommendedBubbleView::GetWindowTitle() const {
  return relaunch_recommended_timer_.GetWindowTitle();
}

bool RelaunchRecommendedBubbleView::ShouldShowCloseButton() const {
  return true;
}

ui::ImageModel RelaunchRecommendedBubbleView::GetWindowIcon() {
  return ui::ImageModel::FromVectorIcon(
      vector_icons::kBusinessIcon, ui::kColorIcon,
      ChromeLayoutProvider::Get()->GetDistanceMetric(
          DISTANCE_BUBBLE_HEADER_VECTOR_ICON_SIZE));
}

void RelaunchRecommendedBubbleView::Init() {
  SetLayoutManager(std::make_unique<views::FillLayout>());
  auto label = std::make_unique<views::Label>(
      l10n_util::GetPluralStringFUTF16(IDS_RELAUNCH_RECOMMENDED_BODY,
                                       BrowserList::GetIncognitoBrowserCount()),
      views::style::CONTEXT_DIALOG_BODY_TEXT);

  label->SetMultiLine(true);
  label->SetHorizontalAlignment(gfx::ALIGN_LEFT);

  // Align the body label with the left edge of the bubble's title.
  // TODO(bsep): Remove this when fixing https://crbug.com/810970.
  // Note: BubleFrameView applies INSETS_DIALOG_TITLE either side of the icon.
  const int title_offset = 2 * views::LayoutProvider::Get()
                                   ->GetInsetsMetric(views::INSETS_DIALOG_TITLE)
                                   .left() +
                           ChromeLayoutProvider::Get()->GetDistanceMetric(
                               DISTANCE_BUBBLE_HEADER_VECTOR_ICON_SIZE);
  label->SetBorder(views::CreateEmptyBorder(
      gfx::Insets::TLBR(0, title_offset - margins().left(), 0, 0)));

  AddChildView(std::move(label));

  base::RecordAction(base::UserMetricsAction("RelaunchRecommendedShown"));
}

void RelaunchRecommendedBubbleView::VisibilityChanged(
    views::View* starting_from,
    bool is_visible) {
  views::InkDrop::Get(GetAnchorView())
      ->AnimateToState(is_visible ? views::InkDropState::ACTIVATED
                                  : views::InkDropState::DEACTIVATED,
                       nullptr);
}

// |relaunch_recommended_timer_| automatically starts for the next time the
// title needs to be updated (e.g., from "2 days" to "3 days").
RelaunchRecommendedBubbleView::RelaunchRecommendedBubbleView(
    views::Button* anchor_button,
    base::Time detection_time,
    base::RepeatingClosure on_accept)
    : LocationBarBubbleDelegateView(anchor_button, nullptr, /*autosize=*/true),
      on_accept_(std::move(on_accept)),
      relaunch_recommended_timer_(
          detection_time,
          base::BindRepeating(&RelaunchRecommendedBubbleView::UpdateWindowTitle,
                              base::Unretained(this))) {
  SetButtons(static_cast<int>(ui::mojom::DialogButton::kOk));
  SetButtonLabel(ui::mojom::DialogButton::kOk,
                 l10n_util::GetStringUTF16(IDS_RELAUNCH_ACCEPT_BUTTON));
  SetShowIcon(true);

  SetCloseCallback(
      base::BindOnce(&base::RecordAction,
                     base::UserMetricsAction("RelaunchRecommended_Close")));

  set_fixed_width(views::LayoutProvider::Get()->GetDistanceMetric(
      views::DISTANCE_BUBBLE_PREFERRED_WIDTH));

  set_margins(ChromeLayoutProvider::Get()->GetDialogInsetsForContentType(
      views::DialogContentType::kText, views::DialogContentType::kText));
}

void RelaunchRecommendedBubbleView::UpdateWindowTitle() {
  // `UpdateWindowTitle` will `InvalidateLayout` when necessary.
  GetWidget()->UpdateWindowTitle();
}
