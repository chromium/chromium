// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/reduce_accept_language/browser/reduce_accept_language_service_test_util.h"

#include <memory>
#include <string>
#include <vector>

#include "base/test/metrics/histogram_tester.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/prefs/pref_service.h"
#include "components/reduce_accept_language/browser/reduce_accept_language_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

using reduce_accept_language::ReduceAcceptLanguageService;

namespace reduce_accept_language::test {

ReduceAcceptLanguageServiceTester::ReduceAcceptLanguageServiceTester(
    HostContentSettingsMap* settings_map,
    ReduceAcceptLanguageService* service,
    PrefService* prefs)
    : settings_map_(settings_map), service_(service), prefs_(prefs) {}

void ReduceAcceptLanguageServiceTester::VerifyFetchAcceptLanguageList(
    const std::vector<std::string>& expected_languages) const {
  const std::vector<std::string>& persisted_languages =
      service_->GetUserAcceptLanguages();
  EXPECT_EQ(persisted_languages, expected_languages);
}

void ReduceAcceptLanguageServiceTester::VerifyPersistFail(
    const GURL& host,
    const std::string& language) const {
  service_->PersistReducedLanguage(url::Origin::Create(host), language);

  const std::optional<std::string>& persisted_language =
      service_->GetReducedLanguage(url::Origin::Create(host));
  EXPECT_FALSE(persisted_language.has_value());
}

void ReduceAcceptLanguageServiceTester::VerifyPersistSuccessOnJavaScriptDisable(
    const GURL& host,
    const std::string& language) const {
  settings_map_->SetContentSettingDefaultScope(
      host, GURL(), ContentSettingsType::JAVASCRIPT, CONTENT_SETTING_BLOCK);
  VerifyPersistSuccess(host, language);

  // Clear settings map changes to avoid side effects for other tests.
  settings_map_->SetWebsiteSettingDefaultScope(
      host, GURL(), ContentSettingsType::JAVASCRIPT, base::Value());
}

void ReduceAcceptLanguageServiceTester::VerifyPersistSuccess(
    const GURL& host,
    const std::string& language) const {
  base::HistogramTester histograms;

  url::Origin origin = url::Origin::Create(host);
  service_->PersistReducedLanguage(origin, language);

  const std::optional<std::string>& persisted_language =
      service_->GetReducedLanguage(origin);
  EXPECT_TRUE(persisted_language.has_value());
  EXPECT_EQ(persisted_language.value(), language);

  histograms.ExpectTotalCount("ReduceAcceptLanguage.StoreLatency", 1);
  histograms.ExpectUniqueSample("ReduceAcceptLanguage.UpdateSize",
                                language.size(), 1);

  service_->ClearReducedLanguage(origin);
  EXPECT_FALSE(service_->GetReducedLanguage(origin).has_value());
}

void ReduceAcceptLanguageServiceTester::VerifyPersistMultipleHostsSuccess(
    const std::vector<GURL>& hosts,
    const std::vector<std::string>& languages) const {
  EXPECT_EQ(hosts.size(), languages.size());

  for (size_t i = 0; i < hosts.size(); i++) {
    service_->PersistReducedLanguage(url::Origin::Create(hosts[i]),
                                     languages[i]);

    const std::optional<std::string>& persisted_language =
        service_->GetReducedLanguage(url::Origin::Create(hosts[i]));
    EXPECT_TRUE(persisted_language.has_value());
    EXPECT_EQ(persisted_language.value(), languages[i]);
  }

  // Clear first origin storage and verify the first origin has no persisted
  // language.
  url::Origin clear_origin = url::Origin::Create(hosts[0]);
  service_->ClearReducedLanguage(clear_origin);
  EXPECT_FALSE(service_->GetReducedLanguage(clear_origin).has_value());

  // Verify other origins still have the persisted language.
  for (size_t i = 1; i < hosts.size(); i++) {
    const std::optional<std::string>& persisted_language =
        service_->GetReducedLanguage(url::Origin::Create(hosts[i]));
    EXPECT_TRUE(persisted_language.has_value());
    EXPECT_EQ(persisted_language.value(), languages[i]);

    // Clear persisted language to avoid side effects for other tests.
    service_->ClearReducedLanguage(url::Origin::Create(hosts[i]));
  }
}

}  // namespace reduce_accept_language::test
