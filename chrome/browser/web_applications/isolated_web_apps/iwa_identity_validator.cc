// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/isolated_web_apps/iwa_identity_validator.h"

#include "base/base64.h"
#include "base/containers/map_util.h"
#include "base/no_destructor.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/web_applications/isolated_web_apps/key_distribution/iwa_key_distribution_info_provider.h"
#include "components/web_package/signed_web_bundles/ecdsa_p256_public_key.h"
#include "components/web_package/signed_web_bundles/ed25519_public_key.h"

namespace web_app {

namespace {

base::expected<void, std::string>
ValidateWebBundleIdentityAgainstKeyRotationInfo(
    const std::string& web_bundle_id,
    const std::vector<web_package::PublicKey>& public_keys,
    const IwaKeyDistributionInfoProvider::KeyRotationInfo& kr_info) {
  if (!kr_info.public_key) {
    return base::unexpected(base::StringPrintf(
        "Web Bundle ID <%s> is disabled via the Key Rotation Component.",
        web_bundle_id.c_str()));
  }

  if (!base::ranges::any_of(public_keys, [&](const auto& public_key) {
        return absl::visit(
            [&](const auto& public_key) {
              return base::ranges::equal(public_key.bytes(),
                                         *kr_info.public_key);
            },
            public_key);
      })) {
    return base::unexpected(
        base::StringPrintf("Rotated key for Web Bundle ID <%s> doesn't match "
                           "any public key in the signature list.",
                           web_bundle_id.c_str()));
  }

  return base::ok();
}

}  // namespace

void IwaIdentityValidator::CreateSingleton() {
  static base::NoDestructor<IwaIdentityValidator> instance;
  instance.get();
}

base::expected<void, std::string>
IwaIdentityValidator::ValidateWebBundleIdentity(
    const std::string& web_bundle_id,
    const std::vector<web_package::PublicKey>& public_keys) const {
  if (const auto* kr_info =
          IwaKeyDistributionInfoProvider::GetInstance()->GetKeyRotationInfo(
              web_bundle_id)) {
    return ValidateWebBundleIdentityAgainstKeyRotationInfo(
        web_bundle_id, public_keys, *kr_info);
  }

  return IdentityValidator::ValidateWebBundleIdentity(web_bundle_id,
                                                      public_keys);
}

}  // namespace web_app
