// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webapps/installable/installable_utils.h"

#include "build/build_config.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_ANDROID)
#include "chrome/browser/android/shortcut_helper.h"
#else
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "url/url_constants.h"
#endif

bool DoesOriginContainAnyInstalledWebApp(
    content::BrowserContext* browser_context,
    const GURL& origin) {
  DCHECK_EQ(origin, origin.DeprecatedGetOriginAsURL());
#if BUILDFLAG(IS_ANDROID)
  return ShortcutHelper::DoesOriginContainAnyInstalledWebApk(origin);
#else
  auto* provider = web_app::WebAppProvider::GetForWebApps(
      Profile::FromBrowserContext(browser_context));
  // TODO: Change this method to async, or document that the caller must know
  // that WebAppProvider is started.
  if (!provider || !provider->on_registry_ready().is_signaled())
    return false;
  return provider->registrar_unsafe().DoesScopeContainAnyApp(origin);
#endif
}

std::set<GURL> GetOriginsWithInstalledWebApps(
    content::BrowserContext* browser_context) {
#if BUILDFLAG(IS_ANDROID)
  return ShortcutHelper::GetOriginsWithInstalledWebApksOrTwas();
#else
  auto* provider = web_app::WebAppProvider::GetForWebApps(
      Profile::FromBrowserContext(browser_context));
  // TODO: Change this method to async, or document that the caller must know
  // that WebAppProvider is started.
  if (!provider || !provider->on_registry_ready().is_signaled())
    return std::set<GURL>();
  const web_app::WebAppRegistrar& registrar = provider->registrar_unsafe();
  auto app_ids = registrar.GetAppIds();
  std::set<GURL> installed_origins;
  for (auto& app_id : app_ids) {
    GURL origin = registrar.GetAppScope(app_id).DeprecatedGetOriginAsURL();
    DCHECK(origin.is_valid());
    if (origin.SchemeIs(url::kHttpsScheme)) {
      installed_origins.emplace(origin);
    }
  }
  return installed_origins;
#endif
}
