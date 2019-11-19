// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/extensions/bookmark_app_util.h"

#include "base/strings/string_piece.h"
#include "base/values.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/web_applications/components/app_registrar.h"
#include "chrome/browser/web_applications/components/web_app_provider_base.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/extensions/api/url_handlers/url_handlers_parser.h"
#include "chrome/common/extensions/manifest_handlers/app_launch_info.h"
#include "content/public/browser/browser_context.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/common/extension.h"
#include "url/gurl.h"

namespace extensions {
namespace {

// A preference used to indicate that a bookmark apps is fully locally installed
// on this machine. The default value (i.e. if the pref is not set) is to be
// fully locally installed, so that hosted apps or bookmark apps created /
// synced before this pref existed will be treated as locally installed.
const char kPrefLocallyInstalled[] = "locallyInstalled";

}  // namespace

void SetBookmarkAppIsLocallyInstalled(content::BrowserContext* context,
                                      const Extension* extension,
                                      bool is_locally_installed) {
  ExtensionPrefs::Get(context)->UpdateExtensionPref(
      extension->id(), kPrefLocallyInstalled,
      std::make_unique<base::Value>(is_locally_installed));
}

bool BookmarkAppIsLocallyInstalled(content::BrowserContext* context,
                                   const Extension* extension) {
  return BookmarkAppIsLocallyInstalled(ExtensionPrefs::Get(context), extension);
}

bool BookmarkAppIsLocallyInstalled(const ExtensionPrefs* prefs,
                                   const Extension* extension) {
  bool locally_installed;
  if (prefs->ReadPrefAsBoolean(extension->id(), kPrefLocallyInstalled,
                               &locally_installed)) {
    return locally_installed;
  }

  return true;
}

bool IsInNavigationScopeForLaunchUrl(const GURL& launch_url, const GURL& url) {
  // Drop any "suffix" components after the path (Resolve "."):
  const GURL nav_scope = launch_url.GetWithoutFilename();

  const int scope_str_length = nav_scope.spec().size();
  return base::StringPiece(nav_scope.spec()) ==
         base::StringPiece(url.spec()).substr(0, scope_str_length);
}

const Extension* GetInstalledShortcutForUrl(Profile* profile, const GURL& url) {
  const ExtensionPrefs* prefs = ExtensionPrefs::Get(profile);
  web_app::AppRegistrar& registrar =
      web_app::WebAppProviderBase::GetProviderBase(profile)->registrar();
  for (scoped_refptr<const Extension> app :
       ExtensionRegistry::Get(profile)->enabled_extensions()) {
    if (!app->from_bookmark())
      continue;
    if (!BookmarkAppIsLocallyInstalled(prefs, app.get()))
      continue;
    if (!registrar.IsShortcutApp(app->id()))
      continue;

    const GURL launch_url = AppLaunchInfo::GetLaunchWebURL(app.get());
    if (IsInNavigationScopeForLaunchUrl(launch_url, url))
      return app.get();
  }
  return nullptr;
}

int CountUserInstalledBookmarkApps(content::BrowserContext* browser_context) {
  // To avoid data races and inaccurate counting, ensure that ExtensionSystem is
  // always ready at this point.
  DCHECK(extensions::ExtensionSystem::Get(browser_context)
             ->extension_service()
             ->is_ready());

  int num_user_installed = 0;

  const ExtensionPrefs* prefs = ExtensionPrefs::Get(browser_context);
  for (scoped_refptr<const Extension> app :
       ExtensionRegistry::Get(browser_context)->enabled_extensions()) {
    if (!app->from_bookmark())
      continue;
    if (!BookmarkAppIsLocallyInstalled(prefs, app.get()))
      continue;
    if (!app->was_installed_by_default())
      ++num_user_installed;
  }

  return num_user_installed;
}

}  // namespace extensions
