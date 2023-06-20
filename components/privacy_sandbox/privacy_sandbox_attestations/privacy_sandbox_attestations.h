// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PRIVACY_SANDBOX_PRIVACY_SANDBOX_ATTESTATIONS_PRIVACY_SANDBOX_ATTESTATIONS_H_
#define COMPONENTS_PRIVACY_SANDBOX_PRIVACY_SANDBOX_ATTESTATIONS_PRIVACY_SANDBOX_ATTESTATIONS_H_

#include <memory>
#include <vector>

#include "base/containers/enum_set.h"
#include "base/containers/flat_map.h"
#include "base/no_destructor.h"
#include "net/base/schemeful_site.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace privacy_sandbox {

// When a new enum value is added:
// 1. Update kMaxValue to match it.
// 2. Update `PrivacySandboxAttestationsGatedAPIProto` in
//    `privacy_sandbox_attestations.proto`.
// 3. Update `AllowAPI` in `privacy_sandbox_attestations_parser.cc`.
enum class PrivacySandboxAttestationsGatedAPI {
  kTopics,
  kProtectedAudience,
  kPrivateAggregation,
  kAttributionReporting,
  kSharedStorage,

  kMaxValue = kSharedStorage,
};

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
  static std::unique_ptr<PrivacySandboxAttestations> CreateForTesting();

  static void SetInstanceForTesting(PrivacySandboxAttestations* test_instance);

  ~PrivacySandboxAttestations();

  PrivacySandboxAttestations(const PrivacySandboxAttestations&) = delete;
  PrivacySandboxAttestations(PrivacySandboxAttestations&&);

  PrivacySandboxAttestations& operator=(const PrivacySandboxAttestations&) =
      delete;
  PrivacySandboxAttestations& operator=(PrivacySandboxAttestations&&);

  // Returns whether `site` is enrolled and attested for `invoking_api`.
  // (If the `kEnforcePrivacySandboxAttestations` flag is disabled, returns
  // true unconditionally.)
  bool IsSiteAttested(const net::SchemefulSite& site,
                      PrivacySandboxAttestationsGatedAPI invoking_api) const;

  // Override the site to be attested for all the Privacy Sandbox APIs, even if
  // it is not officially enrolled. This allows developers to test Privacy
  // Sandbox APIs. The overriding is done using the devtools procotol.
  void AddOverride(const net::SchemefulSite& site);
  bool IsOverridden(const net::SchemefulSite& site) const;

  // Tests can directly set the underlying `attestations_map_` through this test
  // only function. Note: tests should call `CreateAndSetForTesting()` before
  // calling this to make sure the attestations map is set to the testing
  // instance.
  void SetAttestationsForTesting(
      PrivacySandboxAttestationsMap attestations_map);

 private:
  // TODO(xiaochenzh): This class should also hold the version of the
  // attestations.
  friend class base::NoDestructor<PrivacySandboxAttestations>;

  // The constructor is private to enforce the singleton requirement of this
  // class.
  PrivacySandboxAttestations();

  // A data structure for storing and checking Privacy Sandbox attestations,
  // i.e. whether particular sites have opted in to using particular Privacy
  // Sandbox APIs. If this is a `nullopt`, this implies the attestations list
  // has not been loaded yet.
  absl::optional<PrivacySandboxAttestationsMap> attestations_map_;
  std::vector<net::SchemefulSite> overridden_sites_;
};

}  // namespace privacy_sandbox

#endif  // COMPONENTS_PRIVACY_SANDBOX_PRIVACY_SANDBOX_ATTESTATIONS_PRIVACY_SANDBOX_ATTESTATIONS_H_
