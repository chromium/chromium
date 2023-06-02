// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/webid/fedcm_modal_dialog_view.h"

#include "components/constrained_window/constrained_window_views.h"
#include "components/url_formatter/elide_url.h"
#include "components/web_modal/web_contents_modal_dialog_manager.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/border.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/table_layout.h"

FedCmModalDialogView::FedCmModalDialogView(
    content::WebContents* web_contents,
    FedCmModalDialogView::Observer* observer)
    : source_window_(web_contents), observer_(observer) {}

FedCmModalDialogView::~FedCmModalDialogView() = default;

content::WebContents* FedCmModalDialogView::ShowPopupWindow(const GURL& url) {
  content::OpenURLParams params(
      url, content::Referrer(), WindowOpenDisposition::NEW_POPUP,
      ui::PAGE_TRANSITION_AUTO_TOPLEVEL, /*is_renderer_initiated=*/false);
  popup_window_ =
      source_window_->GetDelegate()->OpenURLFromTab(source_window_, params);

  constexpr int kPopupWindowWidth = 512;
  constexpr int kPopupWindowHeight = 450;
  gfx::Rect source_window_rect = source_window_->GetContainerBounds();
  int x_coordinate = source_window_rect.x() +
                     ((source_window_rect.width() - kPopupWindowWidth) / 2);
  int y_coordinate = source_window_rect.y();
  popup_window_->GetDelegate()->SetContentsBounds(
      popup_window_, gfx::Rect(x_coordinate, y_coordinate, kPopupWindowWidth,
                               kPopupWindowHeight));

  Observe(popup_window_);

  return popup_window_;
}

void FedCmModalDialogView::ClosePopupWindow() {
  if (!popup_window_) {
    return;
  }

  popup_window_->Close();
}

void FedCmModalDialogView::WebContentsDestroyed() {
  // Let the observer know that the pop-up window has been destroyed.
  if (observer_) {
    observer_->OnPopupWindowDestroyed();
  }
}
