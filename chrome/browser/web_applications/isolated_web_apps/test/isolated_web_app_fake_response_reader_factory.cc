// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/isolated_web_apps/test/isolated_web_app_fake_response_reader_factory.h"

#include <memory>

#include "base/types/expected.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/isolated_web_apps/error/unusable_swbn_file_error.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_validator.h"
#include "chrome/browser/web_applications/isolated_web_apps/signed_web_bundle_reader.h"
#include "chrome/browser/web_applications/isolated_web_apps/test/test_signed_web_bundle_builder.h"
#include "components/web_package/signed_web_bundles/signed_web_bundle_id.h"
#include "components/web_package/signed_web_bundles/signed_web_bundle_signature_verifier.h"
#include "components/web_package/test_support/signed_web_bundles/signature_verifier_test_utils.h"

namespace web_app {

namespace {

web_package::SignedWebBundleIntegrityBlock CreateDummyIntegrityBlock() {
  auto raw_integrity_block = web_package::mojom::BundleIntegrityBlock::New();
  raw_integrity_block->size = 123;

  auto entry =
      web_package::mojom::BundleIntegrityBlockSignatureStackEntry::New();
  entry->signature_info = web_package::mojom::SignatureInfo::NewEd25519(
      web_package::mojom::SignatureInfoEd25519::New());
  entry->signature_info->get_ed25519()->public_key =
      test::GetDefaultEd25519KeyPair().public_key;
  auto signed_web_bundle_id =
      web_package::SignedWebBundleId::CreateForPublicKey(
          test::GetDefaultEd25519KeyPair().public_key);
  raw_integrity_block->signature_stack.push_back(std::move(entry));
  raw_integrity_block->attributes =
      web_package::test::GetAttributesForSignedWebBundleId(
          signed_web_bundle_id.id());

  auto integrity_block = web_package::SignedWebBundleIntegrityBlock::Create(
      std::move(raw_integrity_block));
  CHECK(integrity_block.has_value()) << integrity_block.error();

  return *integrity_block;
}

}  // namespace

web_package::SignedWebBundleIntegrityBlock
MockIsolatedWebAppResponseReader::GetIntegrityBlock() {
  return CreateDummyIntegrityBlock();
}

void MockIsolatedWebAppResponseReader::ReadResponse(
    const network::ResourceRequest& resource_request,
    ReadResponseCallback callback) {}
void MockIsolatedWebAppResponseReader::Close(base::OnceClosure callback) {
  std::move(callback).Run();
}

FakeResponseReaderFactory::FakeResponseReaderFactory(
    Profile& profile,
    base::expected<void, UnusableSwbnFileError> bundle_status)
    : IsolatedWebAppResponseReaderFactory(
          profile,
          nullptr,
          base::BindRepeating(
              []() -> std::unique_ptr<
                       web_package::SignedWebBundleSignatureVerifier> {
                return nullptr;
              })),
      bundle_status_(std::move(bundle_status)) {}

FakeResponseReaderFactory::~FakeResponseReaderFactory() = default;

void FakeResponseReaderFactory::CreateResponseReader(
    const base::FilePath& web_bundle_path,
    const web_package::SignedWebBundleId& web_bundle_id,
    IsolatedWebAppResponseReaderFactory::Flags flags,
    Callback callback) {
  // Signatures _must_ be verified during installation.
  CHECK(!flags.Has(
      IsolatedWebAppResponseReaderFactory::Flag::kSkipSignatureVerification));
  if (!bundle_status_.has_value()) {
    std::move(callback).Run(base::unexpected(bundle_status_.error()));
  } else {
    std::move(callback).Run(
        std::make_unique<MockIsolatedWebAppResponseReader>());
  }
}

}  // namespace web_app
