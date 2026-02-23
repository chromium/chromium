// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/web_app_helpers.h"

#include "base/check_op.h"
#include "base/feature_list.h"
#include "base/no_destructor.h"
#include "base/strings/strcat.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/proto/web_app_install_state.pb.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "components/crx_file/id_util.h"
#include "components/password_manager/content/common/web_ui_constants.h"
#include "components/webapps/common/web_app_id.h"
#include "content/public/common/url_constants.h"
#include "crypto/sha2.h"
#include "third_party/blink/public/mojom/manifest/manifest.mojom.h"
#include "url/gurl.h"
#include "url/origin.h"
#include "url/url_constants.h"

namespace web_app {

// The following string is used to build the directory name for
// shortcuts to chrome applications (the kind which are installed
// from a CRX).  Application shortcuts to URLs use the {host}_{path}
// for the name of this directory.  Hosts can't include an underscore.
// By starting this string with an underscore, we ensure that there
// are no naming conflicts.
const char kCrxAppPrefix[] = "_crx_";

std::string GenerateApplicationNameFromURL(const GURL& url) {
  return base::StrCat({url.host(), "_", url.path()});
}

std::string GenerateApplicationNameFromAppId(const webapps::AppId& app_id) {
  std::string t(kCrxAppPrefix);
  t.append(app_id);
  return t;
}

webapps::AppId GetAppIdFromApplicationName(const std::string& app_name) {
  std::string prefix(kCrxAppPrefix);
  if (app_name.substr(0, prefix.length()) != prefix)
    return std::string();
  return app_name.substr(prefix.length());
}

webapps::AppId GenerateAppId(const std::optional<std::string>& manifest_id_path,
                             const GURL& start_url) {
  if (!manifest_id_path) {
    return GenerateAppIdFromManifestId(
        GenerateManifestIdFromStartUrlOnly(start_url));
  }
  return GenerateAppIdFromManifestId(
      GenerateManifestId(manifest_id_path.value(), start_url));
}

webapps::AppId GenerateAppIdFromManifest(
    const blink::mojom::Manifest& manifest) {
  CHECK(manifest.id.is_valid());
  return GenerateAppIdFromManifestId(manifest.id);
}

webapps::AppId GenerateAppIdFromManifestId(
    const webapps::ManifestId& manifest_id) {
  // The app ID is hashed twice: here and in GenerateId.
  // The double-hashing is for historical reasons and it needs to stay
  // this way for backwards compatibility. (Back then, a web app's input to the
  // hash needed to be formatted like an extension public key.)
  return crx_file::id_util::GenerateId(
      crypto::SHA256HashString(manifest_id.GetWithoutRef().spec()));
}

webapps::ManifestId GenerateManifestIdFromStartUrlOnly(const GURL& start_url) {
  CHECK(start_url.is_valid()) << start_url.spec();
  return webapps::ManifestId(start_url.GetWithoutRef());
}

webapps::ManifestId GenerateManifestId(const std::string& manifest_id_path,
                                       const GURL& start_url) {
  const webapps::ManifestId manifest_id =
      GenerateManifestIdUnsafe(manifest_id_path, start_url);
  CHECK(manifest_id.is_valid())
      << "start_url: " << start_url << ", manifest_id = " << manifest_id_path;
  return manifest_id;
}

webapps::ManifestId GenerateManifestIdUnsafe(
    const std::string& manifest_id_path,
    const GURL& start_url) {
  // When manifest_id_path is specified, the manifest_id is generated from
  // <start_url_origin>/<manifest_id_path>.
  // Note: start_url.DeprecatedGetOriginAsURL().spec() returns the origin ending
  // with slash.
  const GURL manifest_id(start_url.DeprecatedGetOriginAsURL().spec() +
                         manifest_id_path);
  return webapps::ManifestId(manifest_id.GetWithoutRef());
}

namespace {

base::flat_set<std::string>& ValidChromeUrlHosts() {
  static base::NoDestructor<base::flat_set<std::string>> hosts;
  return *hosts.get();
}

}  // namespace

bool IsValidWebAppUrl(const GURL& app_url) {
  if (app_url.is_empty() || app_url.inner_url())
    return false;

  bool allow_extension_apps = true;
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
  // Stop allowing apps to be extension urls when the shortcuts are separated -
  // they can be extension urls instead.
  allow_extension_apps = false;
#endif

  // TODO(crbug.com/40793595): Remove chrome-extension scheme.
  return app_url.SchemeIs(url::kHttpScheme) ||
         app_url.SchemeIs(url::kHttpsScheme) ||
         (allow_extension_apps && app_url.SchemeIs("chrome-extension")) ||
         (app_url.SchemeIs(content::kChromeUIScheme) &&
          ((app_url.GetHost() ==
            password_manager::kChromeUIPasswordManagerHost) ||
           ValidChromeUrlHosts().contains(app_url.GetHost())));
}

base::ScopedClosureRunner AddValidWebAppChromeUrlHostForTesting(  // IN-TEST
    const std::string& host) {
  CHECK(ValidChromeUrlHosts().insert(host).second);
  return base::ScopedClosureRunner(base::BindOnce(
      [](const std::string& host) {
        CHECK(ValidChromeUrlHosts().contains(host));
        ValidChromeUrlHosts().erase(host);
      },
      host));
}

std::optional<webapps::AppId> FindInstalledAppWithUrlInScope(Profile* profile,
                                                             const GURL& url,
                                                             bool window_only) {
  auto* provider = WebAppProvider::GetForLocalAppsUnchecked(profile);
  return provider
             ? provider->registrar_unsafe().FindBestAppWithUrlInScope(
                   url, window_only
                            ? web_app::WebAppFilter::OpensInDedicatedWindow()
                            : web_app::WebAppFilter::InstalledInChrome())
             : std::nullopt;
}

bool IsNonLocallyInstalledAppWithUrlInScope(Profile* profile, const GURL& url) {
  if (auto* provider = WebAppProvider::GetForWebApps(profile)) {
    FindBestAppInScopeOptions options(WebAppFilter::IsSuggestedApp());
    options.eligibility_filter = WebAppFilter::IsAppSurfaceableToUser();
    return provider->registrar_unsafe()
        .FindBestAppWithUrlInScope(url, options)
        .has_value();
  }
  return false;
}

bool LooksLikePlaceholder(const WebApp& app) {
  for (const auto& [install_source, config] :
       app.management_to_external_config_map()) {
    if (config.is_placeholder) {
      return true;
    }
    for (const GURL& install_url : config.install_urls) {
      if (app.untranslated_name() == install_url.spec()) {
        return true;
      }
    }
  }
  return false;
}

}  // namespace web_app
