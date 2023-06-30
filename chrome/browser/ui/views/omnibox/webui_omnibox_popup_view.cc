// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/omnibox/webui_omnibox_popup_view.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "chrome/browser/ui/views/omnibox/rounded_omnibox_results_frame.h"
#include "chrome/browser/ui/views/theme_copying_widget.h"
#include "chrome/browser/ui/webui/omnibox_popup/omnibox_popup_ui.h"
#include "chrome/browser/ui/webui/realbox/realbox_handler.h"
#include "chrome/common/webui_url_constants.h"
#include "components/omnibox/browser/omnibox_view.h"
#include "ui/base/metadata/metadata_impl_macros.h"

WebUIOmniboxPopupView::WebUIOmniboxPopupView(LocationBarView* location_bar_view)
    : views::WebView(location_bar_view->profile()),
      location_bar_view_(location_bar_view),
      widget_(nullptr) {
  set_owned_by_client();

  // Prepare for instantiation of a RealboxHandler that will connect with
  // this omnibox controller.
  OmniboxPopupUI::SetOmniboxController(
      location_bar_view->GetOmniboxView()->controller());
  LoadInitialURL(GURL(chrome::kChromeUIOmniboxPopupURL));
}

WebUIOmniboxPopupView::~WebUIOmniboxPopupView() {
  ReleaseWidget(false);
}

void WebUIOmniboxPopupView::Show() {
  if (!widget_) {
    widget_ = new ThemeCopyingWidget(location_bar_view_->GetWidget());

    views::Widget* parent_widget = location_bar_view_->GetWidget();
    views::Widget::InitParams params(views::Widget::InitParams::TYPE_POPUP);
#if BUILDFLAG(IS_WIN)
    // On Windows use the software compositor to ensure that we don't block
    // the UI thread during command buffer creation. See http://crbug.com/125248
    params.force_software_compositing = true;
#endif
    params.opacity = views::Widget::InitParams::WindowOpacity::kTranslucent;
    params.parent = parent_widget->GetNativeView();
    params.context = parent_widget->GetNativeWindow();

    RoundedOmniboxResultsFrame::OnBeforeWidgetInit(&params, widget_);

    widget_->Init(std::move(params));

    widget_->ShowInactive();

    widget_->SetContentsView(
        std::make_unique<RoundedOmniboxResultsFrame>(this, location_bar_view_));
    widget_->AddObserver(this);

    // TODO(crbug.com/1396174): Should be dynamically sized based on
    // WebContents.
    SetPreferredSize(gfx::Size(640, 480));

    widget_->SetBounds(GetTargetBounds());
  }
}

void WebUIOmniboxPopupView::Hide() {
  ReleaseWidget(true);
}

RealboxHandler* WebUIOmniboxPopupView::GetWebUIHandler() {
  OmniboxPopupUI* omnibox_popup_ui = static_cast<OmniboxPopupUI*>(
      GetWebContents()->GetWebUI()->GetController());
  DCHECK(omnibox_popup_ui->webui_handler());
  return omnibox_popup_ui->webui_handler();
}

void WebUIOmniboxPopupView::OnWidgetDestroyed(views::Widget* widget) {
  // TODO(crbug.com/1445142): Consider restoring if not closed logically by
  // omnibox.
  if (widget == widget_) {
    widget_ = nullptr;
  }
}

gfx::Rect WebUIOmniboxPopupView::GetTargetBounds() const {
  int popup_height = GetPreferredSize().height();

  // Add enough space on the top and bottom so it looks like there is the same
  // amount of space between the text and the popup border as there is in the
  // interior between each row of text.
  popup_height += RoundedOmniboxResultsFrame::GetNonResultSectionHeight();

  // Add 8dp at the bottom for aesthetic reasons. https://crbug.com/1076646
  // It's expected that this space is dead unclickable/unhighlightable space.
  constexpr int kExtraBottomPadding = 8;
  popup_height += kExtraBottomPadding;

  // The rounded popup is always offset the same amount from the omnibox.
  gfx::Rect content_rect = location_bar_view_->GetBoundsInScreen();
  content_rect.Inset(
      -RoundedOmniboxResultsFrame::GetLocationBarAlignmentInsets());
  content_rect.set_height(popup_height);

  // Finally, expand the widget to accommodate the custom-drawn shadows.
  content_rect.Inset(-RoundedOmniboxResultsFrame::GetShadowInsets());
  return content_rect;
}

void WebUIOmniboxPopupView::ReleaseWidget(bool close) {
  if (widget_) {
    // Avoid possibility of dangling raw_ptr by nulling before cleanup.
    views::Widget* widget = widget_;
    widget_ = nullptr;

    widget->RemoveObserver(this);
    if (close) {
      widget->Close();
    }
  }
  CHECK(!IsInObserverList());
}

BEGIN_METADATA(WebUIOmniboxPopupView, views::WebView)
END_METADATA
