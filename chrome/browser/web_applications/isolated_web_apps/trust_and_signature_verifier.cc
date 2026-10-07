// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/isolated_web_apps/trust_and_signature_verifier.h"

#include <optional>
#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/types/expected.h"
#include "base/types/expected_macros.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_trust_checker.h"
#include "components/web_package/signed_web_bundles/signed_web_bundle_id.h"
#include "components/web_package/signed_web_bundles/signed_web_bundle_integrity_block.h"
#include "components/webapps/isolated_web_apps/bundle_operations/bundle_operations.h"
#include "third_party/abseil-cpp/absl/functional/overload.h"

namespace web_app {

namespace {

base::expected<std::optional<web_package::SignedWebBundleIntegrityBlock>,
               std::string>
WrapIntegrityBlock(base::expected<web_package::SignedWebBundleIntegrityBlock,
                                  std::string> result) {
  return result.transform([](web_package::SignedWebBundleIntegrityBlock value) {
    return std::make_optional(std::move(value));
  });
}

}  // namespace

void CheckTrustAndSignatures(
    const web_package::SignedWebBundleId& web_bundle_id,
    const IwaSourceWithMode& location,
    const IwaOperation& operation,
    Profile* profile,
    base::OnceCallback<
        void(base::expected<
             std::optional<web_package::SignedWebBundleIntegrityBlock>,
             std::string>)> callback) {
  RETURN_IF_ERROR(IsolatedWebAppTrustChecker::IsOperationAllowed(
                      *profile, web_bundle_id, location.dev_mode(), operation),
                  [&](const std::string& error) {
                    std::move(callback).Run(base::unexpected(error));
                  });

  std::visit(
      absl::Overload{
          [&](const IwaSourceBundleWithMode& location) {
            CHECK(!web_bundle_id.is_for_proxy_mode());
            ValidateSignedWebBundleSignatures(
                profile, location.path(), web_bundle_id,
                base::BindOnce(&WrapIntegrityBlock).Then(std::move(callback)));
          },
          [&](const IwaSourceProxy& location) {
            CHECK(web_bundle_id.is_for_proxy_mode());
            // Dev mode proxy mode does not use Web Bundles,
            // hence there is no bundle to validate / trust
            // and no signatures to check.
            std::move(callback).Run(base::ok(std::nullopt));
          }},
      location.variant());
}

void CheckTrustAndSignatures(
    const web_package::SignedWebBundleId& web_bundle_id,
    const IwaSourceWithMode& location,
    const IwaOperation& operation,
    Profile* profile,
    base::OnceCallback<void(base::expected<void, std::string>)> callback) {
  CheckTrustAndSignatures(
      web_bundle_id, location, operation, profile,
      base::BindOnce(
          [](base::expected<
              std::optional<web_package::SignedWebBundleIntegrityBlock>,
              std::string> result) {
            return result.transform([](const auto&) -> void {});
          })
          .Then(std::move(callback)));
}

}  // namespace web_app
