// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OMNIBOX_BROWSER_FAKE_AUTOCOMPLETE_CONTROLLER_H_
#define COMPONENTS_OMNIBOX_BROWSER_FAKE_AUTOCOMPLETE_CONTROLLER_H_

#include <memory>
#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/test/task_environment.h"
#include "components/omnibox/browser/autocomplete_controller.h"
#include "components/omnibox/browser/autocomplete_match_type.h"
#include "components/omnibox/browser/fake_autocomplete_provider.h"

class FakeAutocompleteControllerObserver
    : public AutocompleteController::Observer {
 public:
  void OnResultChanged(AutocompleteController* controller,
                       bool default_match_changed) override;
  int on_result_changed_call_count_ = 0;
  bool last_default_match_changed = false;
};

class FakeAutocompleteController : public AutocompleteController {
 public:
  FakeAutocompleteController(
      raw_ptr<base::test::SingleThreadTaskEnvironment> task_environment);
  ~FakeAutocompleteController() override;

  // Getter for `providers_`.
  template <typename T = FakeAutocompleteProvider>
  T& GetFakeProvider(size_t index = 0) {
    DCHECK_LT(index, providers_.size());
    return *static_cast<T*>(providers_[index].get());
  }

  // Helper to create `AutocompleteInput`.
  static AutocompleteInput CreateInput(
      std::u16string text,
      bool omit_async = false,
      bool prevent_inline_autocomplete = false);

  // Simulates an autocomplete pass.
  // - Also verifies exactly 1 notification occurs after the pass, at the
  //   correct time (which depends on the type of pass simulated), and no
  //   additional notification occurred. This means `SimulateAutocompletePass()`
  //   waits for the notification. So if the test needs to interrupt the
  //   notification, it can't use `SimulateAutocompletePass()`.
  // - Returns the match contents.
  // - Does not reset `internal_result_`. Can be used to simulate a sequence
  //   of passes.
  // - `input` is only used if `sync` is true.
  std::vector<std::string> SimulateAutocompletePass(
      bool sync,
      bool done,
      std::vector<AutocompleteMatch> matches,
      AutocompleteInput input = CreateInput(u"test"));

  // Simulates a `kSyncPassOnly` (the simplest pass) with a clean
  // `internal_result_`. See `SimulateAutocompletePass()`'s comment.
  std::vector<std::string> SimulateCleanAutocompletePass(
      std::vector<AutocompleteMatch> matches);

  // Simulates the expire pass.
  // - Also verifies exactly 1 notification occur at the correct time, and no
  //   additional notification occurred. This means `SimulateExpirePass()` waits
  //   for the notification. So if the test needs to interrupt the notification,
  //   it can't use `SimulateExpirePass()`.
  // - Returns the match contents.
  std::vector<std::string> SimulateExpirePass();

  // Helper to get minimal representation of the controller results: a vector of
  // each match's contents.
  std::vector<std::string> GetResultContents(bool published);

  // Verifies the controller sends a single notification at `delay_ms`, and no
  // notification before.
  void ExpectOnResultChanged(
      int delay_ms,
      AutocompleteController::UpdateType last_update_type);

  // Verifies the controller stops after `delay_ms` with no notification.
  void ExpectStopAfter(int delay_ms);

  // Verifies neither a notification nor stop occur.
  void ExpectNoNotificationOrStop();

  // AutocompleteController (structs):
  using AutocompleteController::OldResult;

  // AutocompleteController (methods):
  using AutocompleteController::MaybeRemoveCompanyEntityImages;
  using AutocompleteController::ShouldRunProvider;
  using AutocompleteController::UpdateResult;

  // AutocompleteController (fields):
  using AutocompleteController::input_;
  using AutocompleteController::internal_result_;
  using AutocompleteController::last_update_type_;
  using AutocompleteController::metrics_;
  using AutocompleteController::providers_;
  using AutocompleteController::published_result_;
  using AutocompleteController::template_url_service_;

  // Used to verify the correct number of notifications occur.
  std::unique_ptr<FakeAutocompleteControllerObserver> observer_;
  // Used to simulate time passing.
  raw_ptr<base::test::SingleThreadTaskEnvironment> task_environment_;
};

#endif  // COMPONENTS_OMNIBOX_BROWSER_FAKE_AUTOCOMPLETE_CONTROLLER_H_
