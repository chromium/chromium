// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/proximity_auth/proximity_auth_error_bubble_view.h"

#include "base/lazy_instance.h"
#include "base/strings/string_util.h"
#include "build/build_config.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/proximity_auth/proximity_auth_error_bubble.h"
#include "chrome/grit/theme_resources.h"
#include "content/public/browser/page_navigator.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/range/range.h"
#include "ui/views/bubble/bubble_border.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/layout/grid_layout.h"
#include "ui/views/widget/widget.h"

using base::WeakPtr;

namespace {

// The bubble's content area width, in device-independent pixels.
const int kBubbleWidth = 296;

// The padding between columns in the bubble.
const int kBubbleIntraColumnPadding = 6;

// The currently visible bubble, or null if there isn't one.
base::LazyInstance<WeakPtr<ProximityAuthErrorBubbleView>>::Leaky g_bubble =
    LAZY_INSTANCE_INITIALIZER;

}  // namespace

void ShowProximityAuthErrorBubble(const base::string16& message,
                                  const gfx::Range& link_range,
                                  const GURL& link_url,
                                  const gfx::Rect& anchor_rect,
                                  content::WebContents* web_contents) {
  // Only one error bubble should be visible at a time.
  // Note that it suffices to check just the |message| for equality, because the
  // |link_range| and |link_url| are always the same for a given |message|
  // value, and there is no way for the bubble's |anchor_rect| to change without
  // dismissing the existing bubble.
  if (g_bubble.Get() && g_bubble.Get()->message() == message)
    return;
  HideProximityAuthErrorBubble();

  g_bubble.Get() = ProximityAuthErrorBubbleView::Create(
      message, link_range, link_url, anchor_rect, web_contents);
  views::BubbleDialogDelegateView::CreateBubble(g_bubble.Get().get())->Show();
}

void HideProximityAuthErrorBubble() {
  if (g_bubble.Get())
    g_bubble.Get()->GetWidget()->Close();
}

// static
WeakPtr<ProximityAuthErrorBubbleView> ProximityAuthErrorBubbleView::Create(
    const base::string16& message,
    const gfx::Range& link_range,
    const GURL& link_url,
    const gfx::Rect& anchor_rect,
    content::WebContents* web_contents) {
  // The created bubble is owned by the views hierarchy.
  ProximityAuthErrorBubbleView* bubble = new ProximityAuthErrorBubbleView(
      message, link_range, link_url, anchor_rect, web_contents);
  return bubble->weak_ptr_factory_.GetWeakPtr();
}

ProximityAuthErrorBubbleView::ProximityAuthErrorBubbleView(
    const base::string16& message,
    const gfx::Range& link_range,
    const GURL& link_url,
    const gfx::Rect& anchor_rect,
    content::WebContents* web_contents)
    : BubbleDialogDelegateView(nullptr, views::BubbleBorder::LEFT_TOP),
      WebContentsObserver(web_contents),
      message_(message),
      link_range_(link_range),
      link_url_(link_url),
      weak_ptr_factory_(this) {
  SetAnchorRect(anchor_rect);
  chrome::RecordDialogCreation(chrome::DialogIdentifier::PROXIMITY_AUTH_ERROR);
}

void ProximityAuthErrorBubbleView::Init() {
  // Define this grid layout for the bubble:
  // ----------------------------
  // | icon | padding | message |
  // ----------------------------
  views::GridLayout* layout =
      SetLayoutManager(std::make_unique<views::GridLayout>(this));
  views::ColumnSet* columns = layout->AddColumnSet(0);
  columns->AddColumn(views::GridLayout::LEADING, views::GridLayout::LEADING,
                     views::GridLayout::kFixedSize, views::GridLayout::USE_PREF,
                     0, 0);
  columns->AddPaddingColumn(views::GridLayout::kFixedSize,
                            kBubbleIntraColumnPadding);
  columns->AddColumn(views::GridLayout::LEADING, views::GridLayout::LEADING,
                     views::GridLayout::kFixedSize, views::GridLayout::USE_PREF,
                     0, 0);

  // Construct the views.
  std::unique_ptr<views::ImageView> warning_icon(new views::ImageView());
  ui::ResourceBundle& bundle = ui::ResourceBundle::GetSharedInstance();
  warning_icon->SetImage(*bundle.GetImageSkiaNamed(IDR_WARNING));

  std::unique_ptr<views::StyledLabel> label(
      new views::StyledLabel(message_, this));
  if (!link_range_.is_empty()) {
    label->AddStyleRange(link_range_,
                         views::StyledLabel::RangeStyleInfo::CreateForLink());
  }
  label->SizeToFit(kBubbleWidth - margins().width() -
                   warning_icon->size().width() - kBubbleIntraColumnPadding);

  // Lay out the views.
  layout->StartRow(views::GridLayout::kFixedSize, 0);
  layout->AddView(warning_icon.release());
  layout->AddView(label.release());
}

ProximityAuthErrorBubbleView::~ProximityAuthErrorBubbleView() {}

int ProximityAuthErrorBubbleView::GetDialogButtons() const {
  return ui::DIALOG_BUTTON_NONE;
}

void ProximityAuthErrorBubbleView::WebContentsDestroyed() {
  GetWidget()->Close();
}

void ProximityAuthErrorBubbleView::StyledLabelLinkClicked(
    views::StyledLabel* label,
    const gfx::Range& range,
    int event_flags) {
  if (!web_contents())
    return;

  web_contents()->OpenURL(content::OpenURLParams(
      link_url_, content::Referrer(), WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui::PAGE_TRANSITION_LINK, false));
}
