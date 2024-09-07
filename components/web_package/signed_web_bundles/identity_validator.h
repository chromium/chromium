// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEB_PACKAGE_SIGNED_WEB_BUNDLES_IDENTITY_VALIDATOR_H_
#define COMPONENTS_WEB_PACKAGE_SIGNED_WEB_BUNDLES_IDENTITY_VALIDATOR_H_

#include <string>
#include <vector>

#include "base/no_destructor.h"
#include "base/types/expected.h"
#include "base/version.h"
#include "components/web_package/signed_web_bundles/types.h"
#include "third_party/abseil-cpp/absl/types/variant.h"

namespace web_package {

namespace test {
class SignedWebBundleSignatureVerifierTestBase;
class SignedWebBundleSignatureVerifierGoToolTest;
}  // namespace test

class IdentityValidator {
 public:
  IdentityValidator(const IdentityValidator&) = delete;
  IdentityValidator& operator=(const IdentityValidator&) = delete;

  static IdentityValidator* GetInstance();

  // Reports whether the inferred web bundle ID is credible.
  virtual base::expected<void, std::string> ValidateWebBundleIdentity(
      const std::string& web_bundle_id,
      const std::vector<PublicKey>& public_keys) const;

 protected:
  IdentityValidator();
  virtual ~IdentityValidator();

 private:
  friend base::NoDestructor<IdentityValidator>;

  friend class test::SignedWebBundleSignatureVerifierTestBase;
  friend class test::SignedWebBundleSignatureVerifierGoToolTest;

  static void CreateInstanceForTesting();
};

}  // namespace web_package

#endif  // COMPONENTS_WEB_PACKAGE_SIGNED_WEB_BUNDLES_IDENTITY_VALIDATOR_H_
