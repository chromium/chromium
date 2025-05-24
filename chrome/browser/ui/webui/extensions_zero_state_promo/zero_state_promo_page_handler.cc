// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/extensions_zero_state_promo/zero_state_promo_page_handler.h"

#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_window.h"
#include "components/url_formatter/url_formatter.h"
#include "mojo/public/cpp/bindings/message.h"
#include "ui/base/mojom/window_open_disposition.mojom.h"
#include "ui/base/window_open_disposition.h"
#include "url/gurl.h"

ZeroStatePromoPageHandler::ZeroStatePromoPageHandler(
    Profile* profile,
    mojo::PendingReceiver<zero_state_promo::mojom::PageHandler> receiver)
    : receiver_(this, std::move(receiver)), profile_(profile) {}

ZeroStatePromoPageHandler::~ZeroStatePromoPageHandler() {}

void ZeroStatePromoPageHandler::LaunchWebStoreLink(
    zero_state_promo::mojom::WebStoreLinkClicked link) {
  GURL url;
  switch (link) {
    case zero_state_promo::mojom::WebStoreLinkClicked::kDiscoverExtension:
      url = GURL(zero_state_promo::mojom::kDiscoverExtensionWebStoreUrl);
      break;
    case zero_state_promo::mojom::WebStoreLinkClicked::kCoupon:
      url = GURL(zero_state_promo::mojom::kCouponWebStoreUrl);
      break;
    case zero_state_promo::mojom::WebStoreLinkClicked::kWriting:
      url = GURL(zero_state_promo::mojom::kWritingWebStoreUrl);
      break;
    case zero_state_promo::mojom::WebStoreLinkClicked::kProductivity:
      url = GURL(zero_state_promo::mojom::kProductivityWebStoreUrl);
      break;
    case zero_state_promo::mojom::WebStoreLinkClicked::kAi:
      url = GURL(zero_state_promo::mojom::kAiWebStoreUrl);
      break;
  }

  NavigateParams params(profile_, url, ::ui::PAGE_TRANSITION_AUTO_BOOKMARK);
  params.disposition = WindowOpenDisposition::NEW_FOREGROUND_TAB;
  Navigate(&params);
  base::UmaHistogramEnumeration(
      "Extension.ZeroStatePromo.IphActionChromeWebStoreLink", link);
}
