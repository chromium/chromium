// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/identity_internals_ui_browsertest.h"

#include "base/strings/string_number_conversions.h"
#include "base/time/time.h"
#include "chrome/browser/extensions/api/identity/extension_token_key.h"
#include "chrome/browser/extensions/api/identity/identity_api.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"

namespace {

const char kChromeWebStoreId[] = "ahfgeienlihckogmohjhadlkjgocpleb";
const int kOneHour = 3600;
} // namespace

IdentityInternalsUIBrowserTest::IdentityInternalsUIBrowserTest() {}

IdentityInternalsUIBrowserTest::~IdentityInternalsUIBrowserTest() {}

void IdentityInternalsUIBrowserTest::SetupTokenCache(int number_of_tokens) {
  for (int number = 0; number < number_of_tokens; ++number) {
    const std::string token_number = base::NumberToString(number);
    std::string token_id("token");
    token_id += token_number;
    std::string extension_id("extension");
    extension_id += token_number;
    std::string gaia_id("account");
    gaia_id += token_number;
    std::vector<std::string> scopes;
    scopes.emplace_back("scope_1_" + token_number);
    scopes.emplace_back("scope_2_" + token_number);
    AddTokenToCache(token_id, extension_id, gaia_id, scopes, kOneHour);
  }
}

void IdentityInternalsUIBrowserTest::SetupTokenCacheWithStoreApp() {
  std::vector<std::string> scopes;
  scopes.emplace_back("store_scope1");
  scopes.emplace_back("store_scope2");
  AddTokenToCache("store_token", kChromeWebStoreId, "store_account", scopes,
                  kOneHour);
}

void IdentityInternalsUIBrowserTest::AddTokenToCache(
    const std::string& token_id,
    const std::string& extension_id,
    const std::string& gaia_id,
    const std::vector<std::string>& scopes,
    int time_to_live) {
  std::set<std::string> scopes_set(scopes.begin(), scopes.end());
  extensions::IdentityTokenCacheValue token_cache_value =
      extensions::IdentityTokenCacheValue::CreateToken(
          token_id, scopes_set, base::Seconds(time_to_live));

  CoreAccountInfo user_info;
  user_info.account_id = CoreAccountId::FromGaiaId(gaia_id);
  user_info.gaia = gaia_id;
  user_info.email = "user_email_" + gaia_id + "@foo.com";

  extensions::ExtensionTokenKey key(extension_id, user_info, scopes_set);
  extensions::IdentityAPI::GetFactoryInstance()
      ->Get(browser()->profile())
      ->token_cache()
      ->SetToken(key, token_cache_value);
}
