// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_error_page.h"

#include <string>

#include "chrome/browser/web_applications/web_app_utils.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/common/alternative_error_page_override_info.mojom.h"
#include "net/base/net_errors.h"
#include "ui/base/l10n/l10n_util.h"

namespace web_app {

content::mojom::AlternativeErrorPageOverrideInfoPtr
MaybeGetIsolatedWebAppErrorPageInfo(const GURL& url,
                                    content::RenderFrameHost* render_frame_host,
                                    content::BrowserContext* browser_context,
                                    int32_t error_code) {
  std::u16string message;
  switch (error_code) {
    case net::ERR_INTERNET_DISCONNECTED:
      message =
          l10n_util::GetStringUTF16(IDS_ERRORPAGES_HEADING_YOU_ARE_OFFLINE);
      break;
    default:
      // TODO(crbug.com/1434818): Add localized IWA error message.
      message = u"DEFAULT IWA ERROR PAGE";
      break;
  }

  return ConstructWebAppErrorPage(url, render_frame_host, browser_context,
                                  message);
}

}  // namespace web_app
