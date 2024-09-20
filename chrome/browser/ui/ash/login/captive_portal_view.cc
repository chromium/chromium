// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/login/captive_portal_view.h"

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/ash/login/captive_portal_window_proxy.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/ash/components/network/network_handler.h"
#include "chromeos/ash/components/network/network_state.h"
#include "chromeos/ash/components/network/network_state_handler.h"
#include "components/captive_portal/core/captive_portal_detector.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/mojom/ui_base_types.mojom-shared.h"
#include "ui/views/widget/widget_delegate.h"
#include "url/gurl.h"

namespace ash {
namespace {

GURL CaptivePortalStartURL() {
  const NetworkState* default_network =
      NetworkHandler::Get()->network_state_handler()->DefaultNetwork();
  if (!default_network || default_network->probe_url().is_empty()) {
    return GURL(captive_portal::CaptivePortalDetector::kDefaultURL);
  }
  return default_network->probe_url();
}

}  // namespace

CaptivePortalView::CaptivePortalView(Profile* profile,
                                     CaptivePortalWindowProxy* proxy,
                                     const std::string& network_name)
    : SimpleWebViewDialog(profile),
      proxy_(proxy),
      network_name_(network_name) {}

CaptivePortalView::~CaptivePortalView() = default;

void CaptivePortalView::StartLoad() {
  start_url_ = CaptivePortalStartURL();
  SimpleWebViewDialog::StartLoad(start_url_);
}

void CaptivePortalView::NavigationStateChanged(
    content::WebContents* source,
    content::InvalidateTypes changed_flags) {
  SimpleWebViewDialog::NavigationStateChanged(source, changed_flags);

  // Naive way to determine the redirection. This won't be needed after portal
  // detection will be done on the Chrome side.
  GURL url = source->GetLastCommittedURL();
  // Note, `url` will be empty for "client3.google.com/generate_204" page.
  if (!redirected_ && url != GURL() && url != start_url_) {
    redirected_ = true;
    proxy_->OnRedirected(network_name_);
  }
}

void CaptivePortalView::LoadingStateChanged(content::WebContents* source,
                                            bool to_different_document) {
  SimpleWebViewDialog::LoadingStateChanged(source, to_different_document);
  // TODO(nkostylev): Fix case of no connectivity, check HTTP code returned.
  // Disable this heuristic as it has false positives.
  // Relying on just shill portal check to close dialog is fine.
  // if (!is_loading && !redirected_)
  //   proxy_->OnOriginalURLLoaded();
}

std::unique_ptr<views::WidgetDelegate> CaptivePortalView::MakeWidgetDelegate() {
  auto delegate = SimpleWebViewDialog::MakeWidgetDelegate();
  delegate->SetCanResize(false);
  delegate->SetModalType(ui::mojom::ModalType::kSystem);
  delegate->SetShowTitle(true);
  delegate->SetTitle(
      l10n_util::GetStringFUTF16(IDS_LOGIN_CAPTIVE_PORTAL_WINDOW_TITLE,
                                 base::ASCIIToUTF16(network_name_)));
  return delegate;
}

BEGIN_METADATA(CaptivePortalView)
END_METADATA

}  // namespace ash
