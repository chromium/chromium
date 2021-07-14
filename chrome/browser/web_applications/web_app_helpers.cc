// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/components/web_app_helpers.h"

#include "base/base64.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "components/crx_file/id_util.h"
#include "crypto/sha2.h"
#include "extensions/common/constants.h"
#include "url/gurl.h"
#include "url/url_constants.h"

namespace web_app {

namespace {

// The following string is used to build the directory name for
// shortcuts to chrome applications (the kind which are installed
// from a CRX).  Application shortcuts to URLs use the {host}_{path}
// for the name of this directory.  Hosts can't include an underscore.
// By starting this string with an underscore, we ensure that there
// are no naming conflicts.
const char kCrxAppPrefix[] = "_crx_";

}  // namespace

std::string GenerateApplicationNameFromURL(const GURL& url) {
  return base::StrCat({url.host_piece(), "_", url.path_piece()});
}

std::string GenerateApplicationNameFromAppId(const AppId& app_id) {
  std::string t(kCrxAppPrefix);
  t.append(app_id);
  return t;
}

AppId GetAppIdFromApplicationName(const std::string& app_name) {
  std::string prefix(kCrxAppPrefix);
  if (app_name.substr(0, prefix.length()) != prefix)
    return std::string();
  return app_name.substr(prefix.length());
}

static std::string GenerateAppHashFromURL(const GURL& url) {
  return crypto::SHA256HashString(url.spec());
}

std::string GenerateAppIdUnhashed(
    const absl::optional<std::string>& manifest_id,
    const GURL& start_url) {
  // When manifest_id is specified, the app id is generated from
  // <start_url_origin>/<manifest_id>.
  // Note: start_url.GetOrigin().spec() returns the origin ending with slash.
  if (manifest_id.has_value()) {
    GURL app_id(start_url.GetOrigin().spec() + manifest_id.value());
    DCHECK(app_id.is_valid());
    return app_id.spec();
  }
  return start_url.spec();
}

AppId GenerateAppId(const absl::optional<std::string>& manifest_id,
                    const GURL& start_url) {
  return crx_file::id_util::GenerateId(
      crypto::SHA256HashString(GenerateAppIdUnhashed(manifest_id, start_url)));
}

AppId GenerateAppIdFromManifest(const blink::Manifest& manifest) {
  return GenerateAppId(
      manifest.id.has_value()
          ? absl::optional<std::string>(base::UTF16ToUTF8(manifest.id.value()))
          : absl::nullopt,
      manifest.start_url);
}

// Generate the public key for the fake extension that we synthesize to contain
// a web app.
//
// Web apps are not signed, but the public key for an extension doubles as
// its unique identity, and we need one of those. A web app's unique identity
// is its manifest URL, so we hash that (*) to create a public key. There will
// be no corresponding private key, which means that these extensions cannot be
// auto-updated using ExtensionUpdater.
//
// (*) The comment above says that we hash the manifest URL, but in practice,
// it seems that we hash the start URL.
std::string GenerateAppKeyFromURL(const GURL& url) {
  std::string key;
  base::Base64Encode(GenerateAppHashFromURL(url), &key);
  return key;
}

bool IsValidWebAppUrl(const GURL& app_url) {
  if (app_url.is_empty() || app_url.inner_url())
    return false;
  // kExtensionScheme is defined in extensions/common:common_constants. It's ok
  // to depend on it.
  return app_url.SchemeIs(url::kHttpScheme) ||
         app_url.SchemeIs(url::kHttpsScheme) ||
         app_url.SchemeIs(extensions::kExtensionScheme);
}

bool IsValidExtensionUrl(const GURL& app_url) {
  return !app_url.is_empty() && !app_url.inner_url() &&
         app_url.SchemeIs(extensions::kExtensionScheme);
}

absl::optional<AppId> FindInstalledAppWithUrlInScope(Profile* profile,
                                                     const GURL& url,
                                                     bool window_only) {
  auto* provider = WebAppProvider::Get(profile);
  return provider ? provider->registrar().FindInstalledAppWithUrlInScope(
                        url, window_only)
                  : absl::nullopt;
}

}  // namespace web_app
