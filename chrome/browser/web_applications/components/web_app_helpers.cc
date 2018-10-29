// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/components/web_app_helpers.h"

#include "base/base64.h"
#include "base/strings/strcat.h"
#include "components/crx_file/id_util.h"
#include "crypto/sha2.h"
#include "url/gurl.h"

namespace web_app {

std::string GenerateApplicationNameFromURL(const GURL& url) {
  return base::StrCat({url.host_piece(), "_", url.path_piece()});
}

// The following string is used to build the directory name for
// shortcuts to chrome applications (the kind which are installed
// from a CRX).  Application shortcuts to URLs use the {host}_{path}
// for the name of this directory.  Hosts can't include an underscore.
// By starting this string with an underscore, we ensure that there
// are no naming conflicts.
static const char kCrxAppPrefix[] = "_crx_";

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

AppId GenerateAppIdFromURL(const GURL& url) {
  return crx_file::id_util::GenerateId(GenerateAppHashFromURL(url));
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

  return app_url.SchemeIsHTTPOrHTTPS();
}

}  // namespace web_app
