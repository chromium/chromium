// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/sampling_metrics_provider.h"

#include "base/metrics/histogram_macros.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface_iterator.h"
#include "chrome/browser/ui/tabs/public/tab_interface.h"
#include "chrome/browser/web_applications/mojom/user_display_mode.mojom.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_app_tab_helper.h"

namespace web_app {

namespace {

// Decreasing this number will improve accuracy at the expense of more frequent
// client-side work.
constexpr int kTimerIntervalInSeconds = 5 * 60;

}  // namespace

SamplingMetricsProvider::SamplingMetricsProvider() {
  timer_.Start(FROM_HERE, base::Seconds(kTimerIntervalInSeconds),
               base::BindRepeating(&SamplingMetricsProvider::EmitMetrics));
}

SamplingMetricsProvider::~SamplingMetricsProvider() = default;

void SamplingMetricsProvider::EmitMetrics() {
  // Total number of PWAs, including backgrounded, separated by tabbed vs
  // standalone.
  int standalone_pwas_count = 0;
  int tabbed_pwas_count = 0;

  // Whether the foreground window has a PWA as its foreground tab, separated by
  // tabbed vs standalone.
  bool standalone_pwas_in_active_use = false;
  bool tabbed_pwas_in_active_use = false;

  // Number of tabbed PWAs, split by most common configurations.
  int tabbed_pwas_user_display_mode_browser_count = 0;
  int tabbed_pwas_user_display_mode_browser_installed_by_user_count = 0;

  int tabbed_pwas_display_mode_standalone_count = 0;
  int tabbed_pwas_display_mode_standalone_installed_by_user_count = 0;

  for (BrowserWindowInterface* browser : GetAllBrowserWindowInterfaces()) {
    // If this is a standalone app window.
    if (browser->GetAppBrowserController()) {
      ++standalone_pwas_count;

      // TODO(https://crbug.com/358404364): This function does not work on macOS
      // for app windows.
      if (browser->IsActive()) {
        standalone_pwas_in_active_use = true;
      }
    }

    // If this is a PWA-tab in a normal browser window.
    if (browser->GetType() == BrowserWindowInterface::Type::TYPE_NORMAL) {
      auto* provider =
          web_app::WebAppProvider::GetForWebApps(browser->GetProfile());
      auto& registrar = provider->registrar_unsafe();

      for (tabs::TabInterface* tab : browser->GetAllTabInterfaces()) {
        auto* web_app_helper =
            web_app::WebAppTabHelper::FromWebContents(tab->GetContents());
        std::optional<webapps::AppId> app_id = web_app_helper->app_id();
        if (app_id) {
          ++tabbed_pwas_count;
          if (tab->IsInForeground() && browser->IsActive()) {
            tabbed_pwas_in_active_use = true;
          }

          std::optional<mojom::UserDisplayMode> user_display_mode =
              registrar.GetAppUserDisplayMode(*app_id);
          bool installed_by_user = registrar.WasInstalledByUser(*app_id);
          if (user_display_mode == mojom::UserDisplayMode::kBrowser) {
            ++tabbed_pwas_user_display_mode_browser_count;
            if (installed_by_user) {
              ++tabbed_pwas_user_display_mode_browser_installed_by_user_count;
            }
          }

          if (user_display_mode == mojom::UserDisplayMode::kStandalone) {
            ++tabbed_pwas_display_mode_standalone_count;
            if (installed_by_user) {
              ++tabbed_pwas_display_mode_standalone_installed_by_user_count;
            }
          }
        }
      }
    }
  }

  int pwas_count = standalone_pwas_count + tabbed_pwas_count;
  bool pwas_in_active_use =
      standalone_pwas_in_active_use || tabbed_pwas_in_active_use;
  UMA_HISTOGRAM_COUNTS_100("WebApp.Engagement2.Count", pwas_count);
  UMA_HISTOGRAM_BOOLEAN("WebApp.Engagement2.Active", pwas_in_active_use);

  UMA_HISTOGRAM_COUNTS_100("WebApp.Engagement2.Standalone.Count",
                           standalone_pwas_count);
  UMA_HISTOGRAM_COUNTS_100("WebApp.Engagement2.Tabbed.Count",
                           tabbed_pwas_count);
  UMA_HISTOGRAM_BOOLEAN("WebApp.Engagement2.Standalone.Active",
                        standalone_pwas_in_active_use);
  UMA_HISTOGRAM_BOOLEAN("WebApp.Engagement2.Tabbed.Active",
                        tabbed_pwas_in_active_use);

  UMA_HISTOGRAM_COUNTS_100(
      "WebApp.Engagement2.Tabbed.UserDisplayModeBrowser.Count",
      tabbed_pwas_user_display_mode_browser_count);
  UMA_HISTOGRAM_COUNTS_100(
      "WebApp.Engagement2.Tabbed.UserDisplayModeBrowserInstalledByUser.Count",
      tabbed_pwas_user_display_mode_browser_installed_by_user_count);
  UMA_HISTOGRAM_COUNTS_100(
      "WebApp.Engagement2.Tabbed.UserDisplayModeStandalone.Count",
      tabbed_pwas_display_mode_standalone_count);
  UMA_HISTOGRAM_COUNTS_100(
      "WebApp.Engagement2.Tabbed.UserDisplayModeStandaloneInstalledByUser."
      "Count",
      tabbed_pwas_display_mode_standalone_installed_by_user_count);
}

}  // namespace web_app
