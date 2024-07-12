// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/web_package/signed_web_bundles/identity_validator.h"

#include "base/no_destructor.h"
#include "base/ranges/algorithm.h"
#include "base/strings/stringprintf.h"
#include "components/web_package/signed_web_bundles/ecdsa_p256_public_key.h"
#include "components/web_package/signed_web_bundles/ed25519_public_key.h"
#include "components/web_package/signed_web_bundles/signed_web_bundle_id.h"

namespace web_package {

namespace {
IdentityValidator* g_instance = nullptr;
}  // namespace

IdentityValidator::IdentityValidator() {
  CHECK(!g_instance);
  g_instance = this;
}

IdentityValidator::~IdentityValidator() {
  CHECK(g_instance);
  g_instance = nullptr;
}

void IdentityValidator::CreateInstanceForTesting() {
  static base::NoDestructor<IdentityValidator> instance;
  instance.get();
}

// static
IdentityValidator* IdentityValidator::GetInstance() {
  CHECK(g_instance)
      << "IdentityValidator must be initialized by the time of "
         "the call to GetInstance(). Normally this happens in the //chrome "
         "layer via IwaIdentityValidator, although not in the case of unit "
         "tests -- there you need to explicitly call "
         "IwaIdentityValidator::CreateSingleton() in the setup phase.";
  return g_instance;
}

base::expected<void, std::string> IdentityValidator::ValidateWebBundleIdentity(
    const std::string& web_bundle_id,
    const std::vector<PublicKey>& public_keys) const {
  if (!base::ranges::any_of(public_keys, [&](const auto& public_key) {
        return absl::visit(
            [&](const auto& public_key) {
              return SignedWebBundleId::CreateForPublicKey(public_key).id() ==
                     web_bundle_id;
            },
            public_key);
      })) {
    return base::unexpected(base::StringPrintf(
        "Web Bundle ID <%s> doesn't match any public key in the signature "
        "list.",
        web_bundle_id.c_str()));
  }

  return base::ok();
}

}  // namespace web_package
