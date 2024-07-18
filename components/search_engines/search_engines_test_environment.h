// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEARCH_ENGINES_SEARCH_ENGINES_TEST_ENVIRONMENT_H_
#define COMPONENTS_SEARCH_ENGINES_SEARCH_ENGINES_TEST_ENVIRONMENT_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "components/prefs/testing_pref_service.h"
#include "components/search_engines/search_engine_choice/search_engine_choice_service.h"
#include "components/search_engines/template_url_service.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"

namespace search_engines {

// Test helper that makes it easier to create a `TemplateURLService` and a
// `SearchEngineChoiceService`. The caller should not worry about the
// dependencies needed to create those classes.
// `pref_service` and `local_state` can be passed by the caller using `Deps`. It
// will be the caller's responsibility to make sure the incoming pref services
// have the needed registrations.
class SearchEnginesTestEnvironment {
 public:
  struct Deps {
    raw_ptr<sync_preferences::TestingPrefServiceSyncable> pref_service =
        nullptr;
    raw_ptr<TestingPrefServiceSimple> local_state = nullptr;
    base::span<const TemplateURLService::Initializer>
        template_url_service_initializer;
  };

  explicit SearchEnginesTestEnvironment(
      const Deps& deps = {/*pref_service=*/nullptr,
                          /*local_state=*/nullptr,
                          /*template_url_service_initializer=*/{}});
  SearchEnginesTestEnvironment(const SearchEnginesTestEnvironment&) = delete;
  SearchEnginesTestEnvironment& operator=(const SearchEnginesTestEnvironment&) =
      delete;

  ~SearchEnginesTestEnvironment();

  sync_preferences::TestingPrefServiceSyncable& pref_service();
  sync_preferences::TestingPrefServiceSyncable& pref_service() const;

  TestingPrefServiceSimple& local_state();

  SearchEngineChoiceService& search_engine_choice_service();

  // Guaranteed to be non-null unless `ReleaseTemplateURLService` has been
  // called.
  TemplateURLService* template_url_service();
  const TemplateURLService* template_url_service() const;

  [[nodiscard]] std::unique_ptr<TemplateURLService> ReleaseTemplateURLService();

 private:
  // Created when `Deps::local_state` is not provided.
  // Don't access it directly, prefer using `local_state_` instead.
  std::unique_ptr<TestingPrefServiceSimple> owned_local_state_;
  raw_ptr<TestingPrefServiceSimple> local_state_;

  // Created when `Deps::pref_service` is not provided.
  // Don't access it directly, prefer using `pref_service_` instead.
  std::unique_ptr<sync_preferences::TestingPrefServiceSyncable>
      owned_pref_service_;
  raw_ptr<sync_preferences::TestingPrefServiceSyncable> pref_service_;

  std::unique_ptr<SearchEngineChoiceService> search_engine_choice_service_;
  std::unique_ptr<TemplateURLService> template_url_service_;
};

}  // namespace search_engines

#endif  // COMPONENTS_SEARCH_ENGINES_SEARCH_ENGINES_TEST_ENVIRONMENT_H_
