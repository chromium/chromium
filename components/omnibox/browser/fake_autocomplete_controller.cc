// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/fake_autocomplete_controller.h"

#include <memory>
#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/task_environment.h"
#include "components/omnibox/browser/autocomplete_controller.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/omnibox/browser/autocomplete_match_type.h"
#include "components/omnibox/browser/fake_autocomplete_controller.h"
#include "components/omnibox/browser/fake_autocomplete_provider.h"
#include "components/omnibox/browser/fake_autocomplete_provider_client.h"
#include "components/omnibox/browser/omnibox_prefs.h"
#include "components/omnibox/browser/test_scheme_classifier.h"
#include "components/prefs/pref_registry_simple.h"
#include "testing/gtest/include/gtest/gtest.h"

void FakeAutocompleteControllerObserver::OnResultChanged(
    AutocompleteController* controller,
    bool default_match_changed) {
  on_result_changed_call_count_++;
  last_default_match_changed = default_match_changed;
}

FakeAutocompleteController::FakeAutocompleteController(
    raw_ptr<base::test::SingleThreadTaskEnvironment> task_environment)
    : AutocompleteController(std::make_unique<FakeAutocompleteProviderClient>(),
                             0),
      task_environment_(task_environment) {
  omnibox::RegisterProfilePrefs(static_cast<PrefRegistrySimple*>(
      static_cast<FakeAutocompleteProviderClient*>(
          autocomplete_provider_client())
          ->GetPrefs()
          ->DeprecatedGetPrefRegistry()));

  providers_.push_back(base::MakeRefCounted<FakeAutocompleteProvider>(
      AutocompleteProvider::Type::TYPE_BOOKMARK));
  providers_.push_back(base::MakeRefCounted<FakeAutocompleteProvider>(
      AutocompleteProvider::Type::TYPE_BUILTIN));
  providers_.push_back(base::MakeRefCounted<FakeAutocompleteProvider>(
      AutocompleteProvider::Type::TYPE_HISTORY_QUICK));
  providers_.push_back(base::MakeRefCounted<FakeAutocompleteProvider>(
      AutocompleteProvider::Type::TYPE_KEYWORD));
  providers_.push_back(base::MakeRefCounted<FakeAutocompleteProvider>(
      AutocompleteProvider::Type::TYPE_SEARCH));
  providers_.push_back(base::MakeRefCounted<FakeAutocompleteProvider>(
      AutocompleteProvider::Type::TYPE_HISTORY_URL));
  providers_.push_back(base::MakeRefCounted<FakeAutocompleteProvider>(
      AutocompleteProvider::Type::TYPE_DOCUMENT));
  providers_.push_back(base::MakeRefCounted<FakeAutocompleteProvider>(
      AutocompleteProvider::Type::TYPE_HISTORY_CLUSTER_PROVIDER));

  observer_ = std::make_unique<FakeAutocompleteControllerObserver>();
  AddObserver(observer_.get());
}

FakeAutocompleteController::~FakeAutocompleteController() = default;

// static
AutocompleteInput FakeAutocompleteController::CreateInput(
    std::u16string text,
    bool omit_async,
    bool prevent_inline_autocomplete) {
  AutocompleteInput input(text, 0, metrics::OmniboxEventProto::OTHER,
                          TestSchemeClassifier());
  input.set_omit_asynchronous_matches(omit_async);
  input.set_prevent_inline_autocomplete(prevent_inline_autocomplete);
  return input;
}

std::vector<std::string> FakeAutocompleteController::SimulateAutocompletePass(
    bool sync,
    bool done,
    std::vector<AutocompleteMatch> matches,
    AutocompleteInput input) {
  GetFakeProvider().matches_ = matches;
  GetFakeProvider().done_ = done;
  AutocompleteController::UpdateType expected_last_update_type =
      AutocompleteController::UpdateType::kNone;
  EXPECT_EQ(observer_->on_result_changed_call_count_, 0);
  if (sync) {
    Start(input);
    expected_last_update_type =
        done ? AutocompleteController::UpdateType::kSyncPassOnly
             : AutocompleteController::UpdateType::kSyncPass;
  } else {
    OnProviderUpdate(true, &GetFakeProvider());
    expected_last_update_type =
        done ? AutocompleteController::UpdateType::kLastAsyncPass
             : AutocompleteController::UpdateType::kAsyncPass;
  }

  ExpectOnResultChanged(sync || done ? 0 : 200, expected_last_update_type);

  return GetResultContents(true);
}

std::vector<std::string>
FakeAutocompleteController::SimulateCleanAutocompletePass(
    std::vector<AutocompleteMatch> matches) {
  internal_result_.ClearMatches();
  return SimulateAutocompletePass(true, true, matches);
}

std::vector<std::string> FakeAutocompleteController::SimulateExpirePass() {
  EXPECT_EQ(observer_->on_result_changed_call_count_, 0);
  UpdateResult(AutocompleteController::UpdateType::kExpirePass);
  ExpectOnResultChanged(200, AutocompleteController::UpdateType::kExpirePass);
  return GetResultContents(true);
}

std::vector<std::string> FakeAutocompleteController::GetResultContents(
    bool published) {
  std::vector<std::string> names;
  auto& result = published ? published_result_ : internal_result_;
  for (const auto& match : result)
    names.push_back(base::UTF16ToUTF8(match.contents));
  return names;
}

void FakeAutocompleteController::ExpectOnResultChanged(
    int delay_ms,
    AutocompleteController::UpdateType last_update_type) {
  std::string debug =
      "delay_ms: " + base::NumberToString(delay_ms) + ", last_update_type: " +
      AutocompleteController::UpdateTypeToDebugString(last_update_type);
  if (delay_ms) {
    task_environment_->FastForwardBy(base::Milliseconds(delay_ms - 1));
    EXPECT_EQ(observer_->on_result_changed_call_count_, 0) << debug;
    task_environment_->FastForwardBy(base::Milliseconds(1));
    EXPECT_EQ(observer_->on_result_changed_call_count_, 1) << debug;
  } else {
    EXPECT_EQ(observer_->on_result_changed_call_count_, 1) << debug;
  }
  EXPECT_EQ(last_update_type_, last_update_type) << debug;
  observer_->on_result_changed_call_count_ = 0;
}

void FakeAutocompleteController::ExpectStopAfter(int delay_ms) {
  if (delay_ms) {
    task_environment_->FastForwardBy(base::Milliseconds(delay_ms - 1));
    EXPECT_NE(last_update_type_, AutocompleteController::UpdateType::kStop)
        << delay_ms;
    task_environment_->FastForwardBy(base::Milliseconds(1));
  }
  EXPECT_EQ(last_update_type_, AutocompleteController::UpdateType::kStop)
      << delay_ms;
  // Any expected notifications should be verified with
  // `ExpectOnResultChanged()` and not slip through polluting subsequent
  // tests.
  EXPECT_EQ(observer_->on_result_changed_call_count_, 0) << delay_ms;
}

void FakeAutocompleteController::ExpectNoNotificationOrStop() {
  // Cache the `last_update_type_` and override, so that we can verify no new
  // stop updates happen even if the last update was a stop.
  auto last_update_type = last_update_type_;
  last_update_type_ = AutocompleteController::UpdateType::kNone;

  task_environment_->FastForwardBy(base::Milliseconds(10000));
  EXPECT_EQ(observer_->on_result_changed_call_count_, 0);
  EXPECT_NE(last_update_type_, AutocompleteController::UpdateType::kStop);

  last_update_type_ = last_update_type;
}
