// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/components/app_registrar.h"
#include "chrome/browser/web_applications/system_web_apps/test/test_system_web_app_installation.h"
#include "chrome/browser/web_applications/system_web_apps/test/test_system_web_app_url_data_source.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/common/url_constants.h"
#include "ui/webui/webui_allowlist.h"

namespace web_app {

namespace {

enum class WebUIType {
  // kChrome WebUIs registration works by creating a WebUIControllerFactory
  // which then register a URLDataSource to serve resources.
  kChrome,
  // kChromeUntrusted WebUIs don't have WebUIControllers and their
  // URLDataSources need to be registered directly.
  kChromeUntrusted,
};

WebUIType GetWebUIType(const GURL& url) {
  if (url.SchemeIs(content::kChromeUIScheme))
    return WebUIType::kChrome;
  if (url.SchemeIs(content::kChromeUIUntrustedScheme))
    return WebUIType::kChromeUntrusted;
  NOTREACHED();
  return WebUIType::kChrome;
}

// Assumes url is like "chrome://web-app/index.html". Returns "web-app";
// This function is needed because at the time TestSystemWebInstallation is
// initialized, chrome scheme is not yet registered with GURL, so it will be
// parsed as PathURL, resulting in an empty host.
std::string GetDataSourceNameFromSystemAppInstallUrl(const GURL& url) {
  DCHECK(url.SchemeIs(content::kChromeUIScheme));

  const std::string& spec = url.spec();
  size_t p = strlen(content::kChromeUIScheme);

  DCHECK_EQ("://", spec.substr(p, 3));
  p += 3;

  size_t pos_after_host = spec.find("/", p);
  DCHECK(pos_after_host != std::string::npos);

  return spec.substr(p, pos_after_host - p);
}

// Returns the scheme and host from an install URL e.g. for
// chrome-untrusted://web-app/index.html this returns
// chrome-untrusted://web-app/.
std::string GetChromeUntrustedDataSourceNameFromInstallUrl(const GURL& url) {
  DCHECK(url.SchemeIs(content::kChromeUIUntrustedScheme));

  const std::string& spec = url.spec();
  size_t p = strlen(content::kChromeUIUntrustedScheme);
  DCHECK_EQ("://", spec.substr(p, 3));
  p += 3;

  size_t pos_after_host = spec.find("/", p);
  DCHECK(pos_after_host != std::string::npos);

  // The Data Source name must include "/" after the host.
  ++pos_after_host;
  return spec.substr(0, pos_after_host);
}

}  // namespace

TestSystemWebAppInstallation::TestSystemWebAppInstallation(SystemAppType type,
                                                           SystemAppInfo info)
    : type_(type) {
  if (GetWebUIType(info.install_url) == WebUIType::kChrome) {
    auto factory = std::make_unique<TestSystemWebAppWebUIControllerFactory>(
        GetDataSourceNameFromSystemAppInstallUrl(info.install_url));
    content::WebUIControllerFactory::RegisterFactory(factory.get());
    web_ui_controller_factories_.push_back(std::move(factory));
  }

  test_web_app_provider_creator_ = std::make_unique<TestWebAppProviderCreator>(
      base::BindRepeating(&TestSystemWebAppInstallation::CreateWebAppProvider,
                          // base::Unretained is safe here. This callback is
                          // called at TestingProfile::Init, which is at test
                          // startup. TestSystemWebAppInstallation is intended
                          // to have the same lifecycle as the test, it won't be
                          // destroyed before the test finishes.
                          base::Unretained(this), info));
}

TestSystemWebAppInstallation::TestSystemWebAppInstallation() {
  test_web_app_provider_creator_ = std::make_unique<
      TestWebAppProviderCreator>(base::BindRepeating(
      &TestSystemWebAppInstallation::CreateWebAppProviderWithNoSystemWebApps,
      // base::Unretained is safe here. This callback is called
      // at TestingProfile::Init, which is at test startup.
      // TestSystemWebAppInstallation is intended to have the
      // same lifecycle as the test, it won't be destroyed before
      // the test finishes.
      base::Unretained(this)));
}

TestSystemWebAppInstallation::~TestSystemWebAppInstallation() {
  for (auto& factory : web_ui_controller_factories_)
    content::WebUIControllerFactory::UnregisterFactoryForTesting(factory.get());
}

std::unique_ptr<WebApplicationInfo> GenerateWebApplicationInfoForTestApp() {
  auto info = std::make_unique<WebApplicationInfo>();
  // the pwa.html is arguably wrong, but the manifest version uses it
  // incorrectly as well, and it's a lot of work to fix it. App ids are
  // generated from this, and it's important to keep it stable across the
  // installation modes.
  info->start_url = GURL("chrome://test-system-app/pwa.html");
  info->scope = GURL("chrome://test-system-app/");
  info->title = u"Test System App";
  info->theme_color = 0xFF00FF00;
  info->display_mode = blink::mojom::DisplayMode::kStandalone;
  info->open_as_window = true;
  return info;
}

std::unique_ptr<WebApplicationInfo>
GenerateWebApplicationInfoForTestAppUntrusted() {
  auto info = GenerateWebApplicationInfoForTestApp();
  info->start_url = GURL("chrome-untrusted://test-system-app/pwa.html");
  info->scope = GURL("chrome-untrusted://test-system-app/");
  return info;
}

// static
std::unique_ptr<TestSystemWebAppInstallation>
TestSystemWebAppInstallation::SetUpWithoutApps() {
  return base::WrapUnique(new TestSystemWebAppInstallation());
}

// static
std::unique_ptr<TestSystemWebAppInstallation>
TestSystemWebAppInstallation::SetUpTabbedMultiWindowApp() {
  SystemAppInfo terminal_system_app_info(
      "Terminal", GURL("chrome://test-system-app/pwa.html"),
      base::BindRepeating(&GenerateWebApplicationInfoForTestApp));
  terminal_system_app_info.single_window = false;

  return base::WrapUnique(new TestSystemWebAppInstallation(
      SystemAppType::TERMINAL, terminal_system_app_info));
}

// static
std::unique_ptr<TestSystemWebAppInstallation>
TestSystemWebAppInstallation::SetUpStandaloneSingleWindowApp() {
  return base::WrapUnique(new TestSystemWebAppInstallation(
      SystemAppType::SETTINGS,
      SystemAppInfo(
          "OSSettings", GURL("chrome://test-system-app/pwa.html"),
          base::BindRepeating(&GenerateWebApplicationInfoForTestApp))));
}

// static
std::unique_ptr<TestSystemWebAppInstallation>
TestSystemWebAppInstallation::SetUpAppThatReceivesLaunchFiles(
    IncludeLaunchDirectory include_launch_directory) {
  SystemAppInfo media_system_app_info(
      "Media", GURL("chrome://test-system-app/pwa.html"),
      base::BindRepeating(&GenerateWebApplicationInfoForTestApp));

  if (include_launch_directory == IncludeLaunchDirectory::kYes)
    media_system_app_info.include_launch_directory = true;
  else
    media_system_app_info.include_launch_directory = false;

  auto* installation = new TestSystemWebAppInstallation(SystemAppType::MEDIA,
                                                        media_system_app_info);
  installation->RegisterAutoGrantedPermissions(
      ContentSettingsType::FILE_SYSTEM_READ_GUARD);
  installation->RegisterAutoGrantedPermissions(
      ContentSettingsType::FILE_SYSTEM_WRITE_GUARD);

  return base::WrapUnique(installation);
}

// static
std::unique_ptr<TestSystemWebAppInstallation>
TestSystemWebAppInstallation::SetUpAppWithEnabledOriginTrials(
    const OriginTrialsMap& origin_to_trials) {
  SystemAppInfo media_system_app_info(
      "Media", GURL("chrome://test-system-app/pwa.html"),
      base::BindRepeating(&GenerateWebApplicationInfoForTestApp));
  media_system_app_info.enabled_origin_trials = origin_to_trials;

  return base::WrapUnique(new TestSystemWebAppInstallation(
      SystemAppType::MEDIA, media_system_app_info));
}

// static
std::unique_ptr<TestSystemWebAppInstallation>
TestSystemWebAppInstallation::SetUpAppNotShownInLauncher() {
  SystemAppInfo app_info(
      "Test", GURL("chrome://test-system-app/pwa.html"),
      base::BindRepeating(&GenerateWebApplicationInfoForTestApp));
  app_info.show_in_launcher = false;

  return base::WrapUnique(new TestSystemWebAppInstallation(
      SystemAppType::SETTINGS, std::move(app_info)));
}

// static
std::unique_ptr<TestSystemWebAppInstallation>
TestSystemWebAppInstallation::SetUpAppNotShownInSearch() {
  SystemAppInfo app_info(
      "Test", GURL("chrome://test-system-app/pwa.html"),
      base::BindRepeating(&GenerateWebApplicationInfoForTestApp));
  app_info.show_in_search = false;

  return base::WrapUnique(new TestSystemWebAppInstallation(
      SystemAppType::SETTINGS, std::move(app_info)));
}

// static
std::unique_ptr<TestSystemWebAppInstallation>
TestSystemWebAppInstallation::SetUpAppWithAdditionalSearchTerms() {
  SystemAppInfo app_info(
      "Test", GURL("chrome://test-system-app/pwa.html"),
      base::BindRepeating(&GenerateWebApplicationInfoForTestApp));
  app_info.additional_search_terms = {IDS_SETTINGS_SECURITY};

  return base::WrapUnique(new TestSystemWebAppInstallation(
      SystemAppType::SETTINGS, std::move(app_info)));
}

// static
std::unique_ptr<TestSystemWebAppInstallation>
TestSystemWebAppInstallation::SetUpAppThatCapturesNavigation() {
  SystemAppInfo app_info(
      "Test", GURL("chrome://test-system-web-app/pwa.html"),
      base::BindRepeating(&GenerateWebApplicationInfoForTestApp));
  app_info.capture_navigations = true;

  auto* installation = new TestSystemWebAppInstallation(SystemAppType::HELP,
                                                        std::move(app_info));

  // Add a helper system app to test capturing links from it.
  const GURL kInitiatingAppUrl = GURL("chrome://initiating-app/pwa.html");
  installation->extra_apps_.insert_or_assign(
      SystemAppType::SETTINGS,
      SystemAppInfo("Initiating App", kInitiatingAppUrl,
                    base::BindLambdaForTesting([]() {
                      auto info = std::make_unique<WebApplicationInfo>();
                      // the pwa.html is arguably wrong, but the manifest
                      // version uses it incorrectly as well, and it's a lot of
                      // work to fix it. App ids are generated from this, and
                      // it's important to keep it stable across the
                      // installation modes.
                      info->start_url =
                          GURL("chrome://initiating-app/pwa.html");
                      info->scope = GURL("chrome://initiating-app/");
                      info->title = u"Test System App";
                      info->theme_color = 0xFF00FF00;
                      info->display_mode =
                          blink::mojom::DisplayMode::kStandalone;
                      info->open_as_window = true;
                      return info;
                    }

                                               )));
  auto factory = std::make_unique<TestSystemWebAppWebUIControllerFactory>(
      kInitiatingAppUrl.host());
  content::WebUIControllerFactory::RegisterFactory(factory.get());
  installation->web_ui_controller_factories_.push_back(std::move(factory));

  return base::WrapUnique(installation);
}

// static
std::unique_ptr<TestSystemWebAppInstallation>
TestSystemWebAppInstallation::SetUpChromeUntrustedApp() {
  return base::WrapUnique(new TestSystemWebAppInstallation(
      SystemAppType::SETTINGS,
      SystemAppInfo("Test", GURL("chrome-untrusted://test-system-app/pwa.html"),
                    base::BindRepeating(
                        &GenerateWebApplicationInfoForTestAppUntrusted))));
}

// static
std::unique_ptr<TestSystemWebAppInstallation>
TestSystemWebAppInstallation::SetUpNonResizeableAndNonMaximizableApp() {
  SystemAppInfo app_info(
      "Test", GURL("chrome://test-system-app/pwa.html"),
      base::BindRepeating(&GenerateWebApplicationInfoForTestApp));
  app_info.is_resizeable = false;
  app_info.is_maximizable = false;

  return base::WrapUnique(new TestSystemWebAppInstallation(
      SystemAppType::SAMPLE, std::move(app_info)));
}

// static
std::unique_ptr<TestSystemWebAppInstallation>
TestSystemWebAppInstallation::SetUpAppWithBackgroundTask() {
  SystemAppInfo app_info(
      "Test", GURL("chrome://test-system-app/pwa.html"),
      base::BindRepeating(&GenerateWebApplicationInfoForTestApp));
  app_info.timer_info.emplace();
  app_info.timer_info->open_immediately = true;
  app_info.timer_info->period = base::TimeDelta::FromDays(1);
  app_info.timer_info->url = GURL("chrome://test-system-app/page2.html");
  return base::WrapUnique(new TestSystemWebAppInstallation(
      SystemAppType::SETTINGS, std::move(app_info)));
}

// static
std::unique_ptr<TestSystemWebAppInstallation>
TestSystemWebAppInstallation::SetupAppWithAllowScriptsToCloseWindows(
    bool value) {
  SystemAppInfo app_info(
      "Test", GURL("chrome://test-system-app/pwa.html"),
      base::BindRepeating(&GenerateWebApplicationInfoForTestApp));
  /* The default value of allow_scripts_to_close_windows is false. */
  if (value)
    app_info.allow_scripts_to_close_windows = true;
  return base::WrapUnique(new TestSystemWebAppInstallation(
      SystemAppType::SAMPLE, std::move(app_info)));
}

std::unique_ptr<KeyedService>
TestSystemWebAppInstallation::CreateWebAppProvider(SystemAppInfo info,
                                                   Profile* profile) {
  DCHECK(!extra_apps_.contains(type_.value()));

  base::flat_map<SystemAppType, SystemAppInfo> apps(extra_apps_);
  apps.insert_or_assign(type_.value(), info);

  profile_ = profile;
  if (GetWebUIType(info.install_url) == WebUIType::kChromeUntrusted) {
    AddTestURLDataSource(
        GetChromeUntrustedDataSourceNameFromInstallUrl(info.install_url),
        profile);
  }

  auto provider = std::make_unique<TestWebAppProvider>(profile);
  auto system_web_app_manager = std::make_unique<SystemWebAppManager>(profile);
  system_web_app_manager->SetSystemAppsForTesting(apps);
  system_web_app_manager->SetUpdatePolicyForTesting(update_policy_);
  provider->SetSystemWebAppManager(std::move(system_web_app_manager));
  provider->Start();

  const url::Origin app_origin = url::Origin::Create(info.install_url);
  auto* allowlist = WebUIAllowlist::GetOrCreate(profile);
  for (const auto& permission : auto_granted_permissions_)
    allowlist->RegisterAutoGrantedPermission(app_origin, permission);

  return provider;
}

std::unique_ptr<KeyedService>
TestSystemWebAppInstallation::CreateWebAppProviderWithNoSystemWebApps(
    Profile* profile) {
  profile_ = profile;
  auto provider = std::make_unique<TestWebAppProvider>(profile);
  auto system_web_app_manager = std::make_unique<SystemWebAppManager>(profile);
  system_web_app_manager->SetSystemAppsForTesting({});
  system_web_app_manager->SetUpdatePolicyForTesting(update_policy_);
  provider->SetSystemWebAppManager(std::move(system_web_app_manager));
  provider->Start();
  return provider;
}

void TestSystemWebAppInstallation::WaitForAppInstall() {
  base::RunLoop run_loop;
  WebAppProvider::Get(profile_)
      ->system_web_app_manager()
      .on_apps_synchronized()
      .Post(FROM_HERE, base::BindLambdaForTesting([&]() {
              // Wait one execution loop for on_apps_synchronized() to be
              // called on all listeners.
              base::ThreadTaskRunnerHandle::Get()->PostTask(
                  FROM_HERE, run_loop.QuitClosure());
            }));
  run_loop.Run();
}

AppId TestSystemWebAppInstallation::GetAppId() {
  return WebAppProvider::Get(profile_)
      ->system_web_app_manager()
      .GetAppIdForSystemApp(type_.value())
      .value();
}

const GURL& TestSystemWebAppInstallation::GetAppUrl() {
  return WebAppProvider::Get(profile_)->registrar().GetAppStartUrl(GetAppId());
}

SystemAppType TestSystemWebAppInstallation::GetType() {
  return type_.value();
}

void TestSystemWebAppInstallation::RegisterAutoGrantedPermissions(
    ContentSettingsType permission) {
  auto_granted_permissions_.insert(permission);
}

}  // namespace web_app
