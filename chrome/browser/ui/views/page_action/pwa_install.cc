// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/page_action/pwa_install.h"

#include "base/metrics/user_metrics.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/web_applications/web_app_dialog_utils.h"
#include "chrome/browser/ui/web_applications/web_app_dialogs.h"
#include "components/webapps/browser/installable/installable_metrics.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "components/metrics/structured/structured_events.h"
#include "components/metrics/structured/structured_metrics_client.h"
#endif

// TODO(crbug.com/376283433): Migrate the tests from
// https://source.chromium.org/chromium/chromium/src/+/main:chrome/browser/ui/views/page_action/pwa_install_view_browsertest.cc
void ShowPwaInstallDialog(Browser* browser,
                          content::WebContents* web_contents) {
  CHECK(browser);

  base::RecordAction(base::UserMetricsAction("PWAInstallIcon"));

  // Close PWA install IPH if it is showing.
  web_app::PwaInProductHelpState iph_state =
      web_app::PwaInProductHelpState::kNotShown;
  bool install_icon_clicked_after_iph_shown =
      browser->window()->NotifyFeaturePromoFeatureUsed(
          feature_engagement::kIPHDesktopPwaInstallFeature,
          FeaturePromoFeatureUsedAction::kClosePromoIfPresent);
  if (install_icon_clicked_after_iph_shown) {
    iph_state = web_app::PwaInProductHelpState::kShown;
  }

#if BUILDFLAG(IS_CHROMEOS)
  metrics::structured::StructuredMetricsClient::Record(
      metrics::structured::events::v2::cr_os_events::
          AppDiscovery_Browser_OmniboxInstallIconClicked()
              .SetIPHShown(install_icon_clicked_after_iph_shown));
#endif

  web_app::CreateWebAppFromManifest(
      web_contents, webapps::WebappInstallSource::OMNIBOX_INSTALL_ICON,
      base::DoNothing(), iph_state);
}
