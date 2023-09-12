// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/webid/fedcm_modal_dialog_view.h"

#include <algorithm>

#include "base/metrics/histogram_macros.h"
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
  CHECK(!popup_window_);

  if (!url.is_valid() || !url.SchemeIsHTTPOrHTTPS()) {
    UMA_HISTOGRAM_ENUMERATION(
        "Blink.FedCm.IdpSigninStatus.ShowPopupWindowResult",
        ShowPopupWindowResult::kFailedByInvalidUrl);

    return nullptr;
  }

  content::OpenURLParams params(
      url, content::Referrer(), WindowOpenDisposition::NEW_POPUP,
      ui::PAGE_TRANSITION_AUTO_TOPLEVEL, /*is_renderer_initiated=*/false);
  popup_window_ =
      source_window_->GetDelegate()->OpenURLFromTab(source_window_, params);

  if (!popup_window_) {
    UMA_HISTOGRAM_ENUMERATION(
        "Blink.FedCm.IdpSigninStatus.ShowPopupWindowResult",
        ShowPopupWindowResult::kFailedForOtherReasons);

    return nullptr;
  }

  constexpr int kPopupWindowWidth = 500;
  constexpr int kPopupWindowPreferredHeight = 600;
  gfx::Rect source_window_rect = source_window_->GetContainerBounds();
  int popup_window_height =
      std::min(kPopupWindowPreferredHeight,
               static_cast<int>(source_window_rect.height() * 0.8));
  int x_coordinate = source_window_rect.x() +
                     ((source_window_rect.width() - kPopupWindowWidth) / 2);
  int y_coordinate = source_window_rect.y() +
                     ((source_window_rect.height() - popup_window_height) / 2);
  popup_window_->GetDelegate()->SetContentsBounds(
      popup_window_, gfx::Rect(x_coordinate, y_coordinate, kPopupWindowWidth,
                               popup_window_height));

  Observe(popup_window_);

  UMA_HISTOGRAM_ENUMERATION("Blink.FedCm.IdpSigninStatus.ShowPopupWindowResult",
                            ShowPopupWindowResult::kSuccess);

  return popup_window_;
}

void FedCmModalDialogView::ClosePopupWindow() {
  if (!popup_window_) {
    return;
  }

  // Store this in a local variable to avoid triggering the dangling pointer
  // detector.
  content::WebContents* popup = popup_window_;
  popup_window_ = nullptr;
  popup->Close();

  UMA_HISTOGRAM_ENUMERATION(
      "Blink.FedCm.IdpSigninStatus.ClosePopupWindowReason",
      FedCmModalDialogView::ClosePopupWindowReason::kIdpInitiatedClose);
}

void FedCmModalDialogView::WebContentsDestroyed() {
  // Let the observer know that the pop-up window has been destroyed.
  if (observer_) {
    observer_->OnPopupWindowDestroyed();
  }

  UMA_HISTOGRAM_ENUMERATION(
      "Blink.FedCm.IdpSigninStatus.ClosePopupWindowReason",
      FedCmModalDialogView::ClosePopupWindowReason::kPopupWindowDestroyed);
}

FedCmModalDialogView::Observer* FedCmModalDialogView::GetObserverForTesting() {
  return observer_;
}
