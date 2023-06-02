// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_REDUCE_ACCEPT_LANGUAGE_BROWSER_REDUCE_ACCEPT_LANGUAGE_SERVICE_TEST_UTIL_H_
#define COMPONENTS_REDUCE_ACCEPT_LANGUAGE_BROWSER_REDUCE_ACCEPT_LANGUAGE_SERVICE_TEST_UTIL_H_

#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"

class HostContentSettingsMap;
class GURL;
class PrefService;

namespace reduce_accept_language {
class ReduceAcceptLanguageService;
}

namespace reduce_accept_language::test {

class ReduceAcceptLanguageServiceTester {
 public:
  explicit ReduceAcceptLanguageServiceTester(
      HostContentSettingsMap* settings_map,
      ReduceAcceptLanguageService* service,
      PrefService* prefs);

  // Checks whether read accept language lists from language prefs as expected.
  void VerifyFetchAcceptLanguageList(
      const std::vector<std::string>& expected_languages) const;

  // Checks whether JavaScriptAllowed works as expected.
  void VerifyIsJavaScriptAllowed(const GURL& host) const;

  // Checks whether persisted `language` failed when provided host.
  void VerifyPersistFail(const GURL& host, const std::string& language) const;

  // Checks whether persisted `language` success when JavaScript is disabled.
  void VerifyPersistSuccessOnJavaScriptDisable(
      const GURL& host,
      const std::string& language) const;

  // Checks whether persisted `language` success.
  void VerifyPersistSuccess(const GURL& host,
                            const std::string& language) const;

  // Checks whether persisted `languages` success.
  void VerifyPersistMultipleHostsSuccess(
      const std::vector<GURL>& hosts,
      const std::vector<std::string>& languages) const;

 private:
  raw_ptr<HostContentSettingsMap, DanglingUntriaged> settings_map_ = nullptr;
  raw_ptr<ReduceAcceptLanguageService, DanglingUntriaged> service_ = nullptr;
  raw_ptr<PrefService, DanglingUntriaged> prefs_ = nullptr;
};

}  // namespace reduce_accept_language::test

#endif  // COMPONENTS_REDUCE_ACCEPT_LANGUAGE_BROWSER_REDUCE_ACCEPT_LANGUAGE_SERVICE_TEST_UTIL_H_
