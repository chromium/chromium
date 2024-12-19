// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/sampling_metrics_provider.h"

#include "base/containers/contains.h"
#include "base/metrics/histogram_macros.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface_iterator.h"
#include "chrome/browser/ui/tabs/public/tab_interface.h"
#include "chrome/browser/web_applications/daily_metrics_helper.h"
#include "chrome/browser/web_applications/mojom/user_display_mode.mojom.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_app_tab_helper.h"
#include "components/services/app_service/public/cpp/preferred_apps_list.h"
#include "components/webapps/browser/banners/app_banner_manager.h"
#include "components/webapps/browser/banners/installable_web_app_check_result.h"
#include "components/webapps/browser/banners/web_app_banner_data.h"

namespace web_app {

namespace {

// Decreasing this number will improve accuracy at the expense of more frequent
// client-side work.
constexpr int kTimerIntervalInSeconds = 5 * 60;

using IdSet = std::set<webapps::AppId>;

// Emits UKM metrics for the given tab. The tab may be for an app window or a
// normal browser window.
void EmitUkmMetricsForTab(tabs::TabInterface* tab) {
  BrowserWindowInterface* browser = tab->GetBrowserWindowInterface();
  Profile* profile = browser->GetProfile();
  auto* web_app_helper =
      web_app::WebAppTabHelper::FromWebContents(tab->GetContents());
  std::optional<webapps::AppId> app_id = web_app_helper->app_id();
  CHECK(app_id);

  auto* provider = web_app::WebAppProvider::GetForWebApps(profile);
  auto& registrar = provider->registrar_unsafe();
  DailyInteraction interaction;

  interaction.start_url = registrar.GetAppStartUrl(*app_id);
  interaction.installed = registrar.IsInstallState(
      *app_id, {proto::INSTALLED_WITHOUT_OS_INTEGRATION,
                proto::INSTALLED_WITH_OS_INTEGRATION});
  auto install_source =
      provider->registrar_unsafe().GetLatestAppInstallSource(*app_id);
  if (install_source) {
    interaction.install_source = static_cast<int>(*install_source);
  }
  DisplayMode display_mode =
      provider->registrar_unsafe().GetAppEffectiveDisplayMode(*app_id);
  interaction.effective_display_mode = static_cast<int>(display_mode);

#if BUILDFLAG(IS_CHROMEOS)
  if (apps::AppServiceProxyFactory::IsAppServiceAvailableForProfile(profile)) {
    auto* proxy = apps::AppServiceProxyFactory::GetForProfile(profile);
    interaction.captures_links =
        proxy->PreferredAppsList().IsPreferredAppForSupportedLinks(*app_id);
  }
#else
  interaction.captures_links = registrar.CapturesLinksInScope(*app_id);
#endif
  interaction.promotable = !registrar.IsDiyApp(*app_id);

  if (tab->IsInForeground() && browser->IsActive()) {
    interaction.foreground_duration = base::Seconds(kTimerIntervalInSeconds);
  } else {
    interaction.background_duration = base::Seconds(kTimerIntervalInSeconds);
  }
  // Note that with this new sampling approach we are no longer tracking the
  // concept of session, and thus don't fill in num_sessions.
  FlushOldRecordsAndUpdate(interaction, profile);
}

// Checks whether metrics should be emitted. If so, updates `emitted_ids` and
// emits metrics.
void MaybeEmitUkmMetricsForTab(tabs::TabInterface* tab, IdSet& emitted_ids) {
  auto* web_app_helper =
      web_app::WebAppTabHelper::FromWebContents(tab->GetContents());
  std::optional<webapps::AppId> app_id = web_app_helper->app_id();

  // A tab in an app window doesn't necessarily have to be in-scope of that
  // app. In can be out of scope, or simply not have it's navigation committed
  // yet.
  if (!app_id) {
    return;
  }

  // We only emit UKM metrics a single time for a given AppId.
  if (base::Contains(emitted_ids, *app_id)) {
    return;
  }

  auto* provider = web_app::WebAppProvider::GetForWebApps(
      tab->GetBrowserWindowInterface()->GetProfile());
  auto& registrar = provider->registrar_unsafe();
  std::optional<mojom::UserDisplayMode> user_display_mode =
      registrar.GetAppUserDisplayMode(*app_id);
  CHECK(user_display_mode);
  switch (*user_display_mode) {
    case mojom::UserDisplayMode::kBrowser:
      // Emit metrics only if the app is configured to run in a tab
      // window, and is running in a tab.
      if (tab->GetBrowserWindowInterface()->GetType() ==
          BrowserWindowInterface::Type::TYPE_NORMAL) {
        emitted_ids.insert(*app_id);
        EmitUkmMetricsForTab(tab);
      }
      break;
    case mojom::UserDisplayMode::kStandalone:
      // Emit metrics only if the app is configured to run in a standalone
      // window, and is running in a standalone window.
      if (tab->GetBrowserWindowInterface()->GetAppBrowserController()) {
        emitted_ids.insert(*app_id);
        EmitUkmMetricsForTab(tab);
      }
      break;
    case mojom::UserDisplayMode::kTabbed:
      // We don't track metrics for tabbed standalone PWAs.
      break;
  }
}

// Checks whether metrics should be emitted. If so, updates `emitted_ids` and
// emits metrics.
void MaybeEmitUkmMetricsForPromotable(
    tabs::TabInterface* tab,
    const webapps::WebAppBannerData& banner_data,
    IdSet& emitted_ids) {
  const GURL& url = banner_data.manifest().start_url;
  if (base::Contains(emitted_ids, url.spec())) {
    return;
  }

  emitted_ids.insert(url.spec());
  DailyInteraction interaction;
  interaction.start_url = url;
  interaction.installed = false;
  if (!banner_data.manifest().display_override.empty()) {
    interaction.effective_display_mode =
        static_cast<int>(banner_data.manifest().display_override[0]);
  } else {
    interaction.effective_display_mode =
        static_cast<int>(banner_data.manifest().display);
  }
  interaction.promotable = true;
  FlushOldRecordsAndUpdate(interaction,
                           tab->GetBrowserWindowInterface()->GetProfile());
}

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

  IdSet emitted_ukm_ids;
  for (BrowserWindowInterface* browser : GetAllBrowserWindowInterfaces()) {
    // If this is a standalone app window.
    if (browser->GetAppBrowserController()) {
      // A browser may be being closed due to empty tabs. See
      // https://crbug.com/378020140.
      if (!browser->GetActiveTabInterface()) {
        continue;
      }

      ++standalone_pwas_count;

      // TODO(https://crbug.com/358404364): This function does not work on macOS
      // for app windows.
      if (browser->IsActive()) {
        standalone_pwas_in_active_use = true;
      }

      MaybeEmitUkmMetricsForTab(browser->GetActiveTabInterface(),
                                emitted_ukm_ids);
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
          MaybeEmitUkmMetricsForTab(tab, emitted_ukm_ids);
        } else {
          // If the tab does not have an app id, it might be promotable.
          auto* app_banner_manager =
              webapps::AppBannerManager::FromWebContents(tab->GetContents());
          if (app_banner_manager) {
            std::optional<webapps::WebAppBannerData> banner_data =
                app_banner_manager->GetCurrentWebAppBannerData();
            webapps::InstallableWebAppCheckResult installable =
                app_banner_manager->GetInstallableWebAppCheckResult();

            if (banner_data &&
                installable ==
                    webapps::InstallableWebAppCheckResult::kYes_Promotable) {
              MaybeEmitUkmMetricsForPromotable(tab, *banner_data,
                                               emitted_ukm_ids);
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
