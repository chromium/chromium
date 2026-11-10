// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/isolated_web_apps/key_rotation_util.h"

#include <array>
#include <optional>
#include <string_view>

#include "base/auto_reset.h"
#include "chrome/browser/web_applications/model/integrity_block_data.h"
#include "chrome/browser/web_applications/model/isolation_data.h"
#include "components/web_package/signed_web_bundles/ed25519_public_key.h"
#include "components/web_package/signed_web_bundles/ed25519_signature.h"
#include "components/web_package/signed_web_bundles/signed_web_bundle_signature_stack_entry.h"
#include "components/web_package/test_support/signed_web_bundles/ed25519_key_pair.h"
#include "components/webapps/isolated_web_apps/key_distribution/iwa_runtime_data_provider.h"
#include "components/webapps/isolated_web_apps/test_support/fake_iwa_runtime_data_provider.h"
#include "components/webapps/isolated_web_apps/types/iwa_version.h"
#include "components/webapps/isolated_web_apps/types/storage_location.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace web_app {
namespace {

enum class Key { kNone, kK1, kK2 };

class IsInstallationSigningKeyRevokedTest : public ::testing::Test {
 protected:
  const web_package::Ed25519PublicKey& GetKey(Key key) {
    return key == Key::kK1 ? k1_.public_key : k2_.public_key;
  }

  void SetKeyRotation(Key rotated_key, Key previous_key) {
    data_provider_.Update([&](auto& update) {
      update.SetKeyRotations({});
      if (rotated_key == Key::kNone) {
        return;
      }
      std::optional<base::span<const uint8_t>> previous_key_bytes;
      if (previous_key != Key::kNone) {
        previous_key_bytes = GetKey(previous_key).bytes();
      }
      update.AddToKeyRotations(web_bundle_id_, GetKey(rotated_key).bytes(),
                               previous_key_bytes);
    });
  }

  IsolationData CreateIsolationData(Key signing_key) {
    IsolationData::Builder builder(
        IwaStorageOwnedBundle("dir", /*dev_mode=*/false),
        *IwaVersion::Create("1.0.0"));
    if (signing_key != Key::kNone) {
      constexpr std::array<uint8_t, web_package::Ed25519Signature::kLength>
          kSignature = {};
      builder.SetIntegrityBlockData(
          IntegrityBlockData({web_package::SignedWebBundleSignatureInfoEd25519(
              GetKey(signing_key),
              web_package::Ed25519Signature::Create(base::span(kSignature)))}));
    }
    return std::move(builder).Build();
  }

  const web_package::test::Ed25519KeyPair k1_ =
      web_package::test::Ed25519KeyPair::CreateRandom();
  const web_package::test::Ed25519KeyPair k2_ =
      web_package::test::Ed25519KeyPair::CreateRandom();
  const web_package::SignedWebBundleId web_bundle_id_ =
      web_package::SignedWebBundleId::CreateForPublicKey(k1_.public_key);

 private:
  FakeIwaRuntimeDataProvider data_provider_;
  base::AutoReset<IwaRuntimeDataProvider*> resetter_ =
      IwaRuntimeDataProvider::SetInstanceForTesting(&data_provider_);
};

TEST_F(IsInstallationSigningKeyRevokedTest, Table) {
  static constexpr struct {
    std::string_view name;
    Key rotated_key;
    Key previous_key;
    Key installed_key;
    bool expected_revoked;
  } kCases[] = {
      {"NoKeyRotation", Key::kNone, Key::kNone, Key::kK1, false},
      {"InstalledWithRotatedKey", Key::kK2, Key::kNone, Key::kK2, false},
      {"InstalledWithPreviousKey", Key::kK2, Key::kK1, Key::kK1, false},
      {"InstalledWithRevokedKey", Key::kK2, Key::kNone, Key::kK1, true},
      {"NoIntegrityBlockData", Key::kK2, Key::kNone, Key::kNone, true},
  };
  for (const auto& test_case : kCases) {
    SCOPED_TRACE(test_case.name);
    SetKeyRotation(test_case.rotated_key, test_case.previous_key);
    EXPECT_EQ(IsInstallationSigningKeyRevoked(
                  web_bundle_id_, CreateIsolationData(test_case.installed_key)),
              test_case.expected_revoked);
  }
}

}  // namespace
}  // namespace web_app
