// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PRIVACY_SANDBOX_PRIVACY_SANDBOX_ATTESTATIONS_PRIVACY_SANDBOX_ATTESTATIONS_H_
#define COMPONENTS_PRIVACY_SANDBOX_PRIVACY_SANDBOX_ATTESTATIONS_PRIVACY_SANDBOX_ATTESTATIONS_H_

#include <memory>
#include <vector>

#include "components/privacy_sandbox/privacy_sandbox_settings_impl.h"

#include "base/containers/enum_set.h"
#include "base/containers/flat_map.h"
#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/no_destructor.h"
#include "base/task/sequenced_task_runner.h"
#include "base/version.h"
#include "net/base/schemeful_site.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace privacy_sandbox {

const base::FilePath::CharType kSentinelFileName[] =
    FILE_PATH_LITERAL("attestations_sentinel");

using PrivacySandboxAttestationsGatedAPISet =
    base::EnumSet<PrivacySandboxAttestationsGatedAPI,
                  PrivacySandboxAttestationsGatedAPI::kTopics,
                  PrivacySandboxAttestationsGatedAPI::kMaxValue>;

// TODO(crbug.com/1454847): Add a concise representation for "this site is
// attested for all APIs".
using PrivacySandboxAttestationsMap =
    base::flat_map<net::SchemefulSite, PrivacySandboxAttestationsGatedAPISet>;

class PrivacySandboxAttestations {
 public:
  // Returns the singleton instance. If there is a test instance present, return
  // the test instance.
  static PrivacySandboxAttestations* GetInstance();

  // This function constructs a `PrivacySandboxAttestations` and returns a
  // unique pointer to it. Test should use this with
  // `ScopedPrivacySandboxAttestations` to install a scoped test instance, for
  // example:
  //
  // ScopedPrivacySandboxAttestations(
  //   PrivacySandboxAttestations::CreateForTesting())
  //
  // The destructor of `ScopedPrivacySandboxAttestations` will reset the
  // `g_test_instance` back to the previous one. If tests are testing APIs that
  // rely on `GetInstance()`, they must use `ScopedPrivacySandboxAttestations`
  // to set up the test instance first.
  // Note: `PrivacySandboxAttestations` requires that tests have a properly set
  // up task environment. For unit-tests, ensure
  // `content::BrowserTaskEnvironment` is initialized. This is required because
  // the final move assignment of the attestations map is done using the UI
  // thread. For browser tests, wait until the main thread is initialized before
  // calling `CreateForTesting()`.
  static std::unique_ptr<PrivacySandboxAttestations> CreateForTesting();

  static void SetInstanceForTesting(PrivacySandboxAttestations* test_instance);

  ~PrivacySandboxAttestations();

  PrivacySandboxAttestations(const PrivacySandboxAttestations&) = delete;
  PrivacySandboxAttestations(PrivacySandboxAttestations&&);

  PrivacySandboxAttestations& operator=(const PrivacySandboxAttestations&) =
      delete;
  PrivacySandboxAttestations& operator=(PrivacySandboxAttestations&&);

  // Returns whether `site` is enrolled and attested for `invoking_api`.
  // This function returns true unconditionally if
  // 1. The `kEnforcePrivacySandboxAttestations` flag is disabled.
  // 2. Or `is_all_apis_attested_for_testing_` is set to true by
  // `SetAllPrivacySandboxAttestedForTesting()` for testing.
  PrivacySandboxSettingsImpl::Status IsSiteAttested(
      const net::SchemefulSite& site,
      PrivacySandboxAttestationsGatedAPI invoking_api) const;

  // Invoke `LoadAttestationsInternal()` to parse the attestations file
  // asynchronously on the SequencedTaskRunner `task_runner_` in the thread
  // pool. This function should only be invoked with a valid version and
  // `kEnforcePrivacySandboxAttestations` enabled. `installed_file_path` should
  // be the path to the attestations list file.
  void LoadAttestations(base::Version version,
                        base::FilePath installed_file_path);

  // Override the site to be attested for all the Privacy Sandbox APIs, even if
  // it is not officially enrolled. This allows developers to test Privacy
  // Sandbox APIs. The overriding is done using the devtools procotol.
  void AddOverride(const net::SchemefulSite& site);
  bool IsOverridden(const net::SchemefulSite& site) const;

  // Tests can call this function to make all privacy sandbox APIS to be
  // considered attested for any site. This is used to test APIs behaviors not
  // related to attestations.
  void SetAllPrivacySandboxAttestedForTesting(bool all_attested);

  // Tests can directly set the underlying `attestations_map_` through this test
  // only function. Note: tests should call `CreateAndSetForTesting()` before
  // calling this to make sure the attestations map is set to the testing
  // instance.
  void SetAttestationsForTesting(
      absl::optional<PrivacySandboxAttestationsMap> attestations_map);

  base::Version GetVersionForTesting() const;

  // Set the callback to be invoked when attestations map is loaded. The typical
  // usage is to set the callback to `base::RunLoop::QuitClosure()`. Tests then
  // can use `base::RunLoop::Run()`, together with this callback, to make sure
  // the parsing and loading are completed.
  void SetLoadAttestationsDoneCallbackForTesting(base::OnceClosure callback);

  // Set the callback to be invoked when attestations map starts to be parsed.
  // (The parsing will be paused.) The typical usage is to set the callback to
  // `base::RunLoop::QuitClosure()`. Tests then can use `base::RunLoop::Run()`,
  // together with this callback, to inspect state once parsing starts.
  void SetLoadAttestationsParsingStartedCallbackForTesting(
      base::OnceClosure callback);

 private:
  friend class base::NoDestructor<PrivacySandboxAttestations>;

  enum Progress {
    kNotStarted,
    kStarted,
    kFinished,
  };

  // The constructor is private to enforce the singleton requirement of this
  // class.
  PrivacySandboxAttestations();

  // Trigger the opening and parsing of the attestations file. When the parsing
  // is done, store the result to `attestations_map_`. If there is an existing
  // attestations map, only parse if the attestations file has a newer version.
  // This function should only be invoked with a valid version and
  // `kEnforcePrivacySandboxAttestations` enabled. `installed_file_path` should
  // be the path to the attestations list file.
  void LoadAttestationsInternal(base::Version version,
                                base::FilePath installed_file_path);

  // Store the parsed attestations map and its version.
  void SetParsedAttestations(base::Version version,
                             PrivacySandboxAttestationsMap attestations_map);

  // Invoke the `attestations_loaded_callback_` registered by tests, if any.
  void RunLoadAttestationsDoneCallbackForTesting();

  // Invoke the `attestations_parsing_started_callback_` registered by tests,
  // if any. If this function returns true, parsing should be paused (because
  // we're in a test). If it returns false, do nothing.
  bool RunLoadAttestationsParsingStartedCallbackForTesting();

  // Task runner used to execute the file opening and parsing.
  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  // This callback is invoked at the end of the loading of the attestations map.
  base::OnceClosure load_attestations_done_callback_;

  // This callback is invoked when parsing for the attestations map starts.
  base::OnceClosure load_attestations_parsing_started_callback_;

  Progress attestations_parse_progress_ = kNotStarted;

  // The attestations file from the component updater should always carry a
  // valid version. If this is a `nullopt`, this implies the attestations list
  // has not been loaded yet.
  base::Version file_version_;

  // A data structure for storing and checking Privacy Sandbox attestations,
  // i.e. whether particular sites have opted in to using particular Privacy
  // Sandbox APIs. If this is a `nullopt`, this implies the attestations list
  // has not been loaded yet.
  absl::optional<PrivacySandboxAttestationsMap> attestations_map_;

  // Overridden sites by DevTools are considered attested.
  std::vector<net::SchemefulSite> overridden_sites_;

  // If true, all Privacy Sandbox APIs are considered attested for any site.
  bool is_all_apis_attested_for_testing_ = false;
};

}  // namespace privacy_sandbox

#endif  // COMPONENTS_PRIVACY_SANDBOX_PRIVACY_SANDBOX_ATTESTATIONS_PRIVACY_SANDBOX_ATTESTATIONS_H_
