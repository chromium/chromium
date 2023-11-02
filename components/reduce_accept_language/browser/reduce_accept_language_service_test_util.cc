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
    const std::vector<std::string>& expected_langauges) const {
  const std::vector<std::string>& languages =
      service_->GetUserAcceptLanguages();
  EXPECT_EQ(languages, expected_langauges);
}

void ReduceAcceptLanguageServiceTester::VerifyPersistFail(
    const GURL& host,
    const std::string& lang) const {
  service_->PersistReducedLanguage(url::Origin::Create(host), lang);

  const absl::optional<std::string>& language =
      service_->GetReducedLanguage(url::Origin::Create(host));
  EXPECT_FALSE(language.has_value());
}

void ReduceAcceptLanguageServiceTester::VerifyPersistSuccessOnJavaScriptDisable(
    const GURL& host,
    const std::string& lang) const {
  settings_map_->SetContentSettingDefaultScope(
      host, GURL(), ContentSettingsType::JAVASCRIPT, CONTENT_SETTING_BLOCK);
  VerifyPersistSuccess(host, lang);
}

void ReduceAcceptLanguageServiceTester::VerifyPersistSuccess(
    const GURL& host,
    const std::string& lang) const {
  base::HistogramTester histograms;
  service_->PersistReducedLanguage(url::Origin::Create(host), lang);

  const absl::optional<std::string>& language =
      service_->GetReducedLanguage(url::Origin::Create(host));
  EXPECT_TRUE(language.has_value());
  EXPECT_EQ(language.value(), lang);

  histograms.ExpectTotalCount("ReduceAcceptLanguage.StoreLatency", 1);
  histograms.ExpectUniqueSample("ReduceAcceptLanguage.UpdateSize", lang.size(),
                                1);
}

void ReduceAcceptLanguageServiceTester::VerifyPersistMultipleHostsSuccess(
    const std::vector<GURL>& hosts,
    const std::vector<std::string>& langs) const {
  EXPECT_EQ(hosts.size(), langs.size());

  for (size_t i = 0; i < hosts.size(); i++) {
    service_->PersistReducedLanguage(url::Origin::Create(hosts[i]), langs[i]);

    const absl::optional<std::string>& language =
        service_->GetReducedLanguage(url::Origin::Create(hosts[i]));
    EXPECT_TRUE(language.has_value());
    EXPECT_EQ(language.value(), langs[i]);
  }
}

}  // namespace reduce_accept_language::test