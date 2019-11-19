// Copyright 2013 The Chromium Authors. All rights reserved.
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
    std::vector<std::string> scopes;
    scopes.push_back(std::string("scope_1_") + token_number);
    scopes.push_back(std::string("scope_2_") + token_number);
    AddTokenToCache(token_id, extension_id, scopes, kOneHour);
  }
}

void IdentityInternalsUIBrowserTest::SetupTokenCacheWithStoreApp() {
  std::vector<std::string> scopes;
  scopes.push_back(std::string("store_scope1"));
  scopes.push_back(std::string("store_scope2"));
  AddTokenToCache("store_token", kChromeWebStoreId, scopes, kOneHour);
}

void IdentityInternalsUIBrowserTest::AddTokenToCache(
    const std::string token_id,
    const std::string extension_id,
    const std::vector<std::string>& scopes,
    int time_to_live) {
  extensions::IdentityTokenCacheValue token_cache_value =
      extensions::IdentityTokenCacheValue(token_id,
          base::TimeDelta::FromSeconds(time_to_live));
  extensions::ExtensionTokenKey key(
      extension_id, CoreAccountId("account_id"),
      std::set<std::string>(scopes.begin(), scopes.end()));
  extensions::IdentityAPI::GetFactoryInstance()
      ->Get(browser()->profile())
      ->SetCachedToken(key, token_cache_value);
}
