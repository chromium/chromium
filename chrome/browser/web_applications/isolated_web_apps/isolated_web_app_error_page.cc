// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_error_page.h"

#include <string>

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/web_applications/web_app_utils.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/common/alternative_error_page_override_info.mojom.h"
#include "net/base/net_errors.h"
#include "ui/base/l10n/l10n_util.h"

namespace {

std::u16string GetNetErrorMessage(net::Error error_code) {
  switch (error_code) {
    case net::ERR_INVALID_WEB_BUNDLE:
      return l10n_util::GetStringUTF16(
          IDS_ERRORPAGES_MESSAGE_IWA_INVALID_WEB_BUNDLE);
    case net::ERR_CONNECTION_REFUSED:
      return l10n_util::GetStringUTF16(
          IDS_ERRORPAGES_MESSAGE_IWA_CONNECTION_REFUSED);
    default:
      break;
  }

  return base::UTF8ToUTF16(net::ErrorToString(error_code));
}

}  // namespace

namespace web_app {

content::mojom::AlternativeErrorPageOverrideInfoPtr
MaybeGetIsolatedWebAppErrorPageInfo(const GURL& url,
                                    content::RenderFrameHost* render_frame_host,
                                    content::BrowserContext* browser_context,
                                    int32_t error_code) {
  return ConstructWebAppErrorPage(
      url, render_frame_host, browser_context,
      GetNetErrorMessage(static_cast<net::Error>(error_code)),
      /*supplementary_icon=*/std::u16string());
}

}  // namespace web_app
