// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/isolated_web_apps/test/isolated_web_app_fake_response_reader_factory.h"

#include <memory>

#include "base/types/expected.h"
#include "chrome/browser/web_applications/isolated_web_apps/error/unusable_swbn_file_error.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_validator.h"
#include "chrome/browser/web_applications/isolated_web_apps/signed_web_bundle_reader.h"
#include "components/web_package/signed_web_bundles/signed_web_bundle_signature_verifier.h"

namespace web_app {

void MockIsolatedWebAppResponseReader::ReadResponse(
    const network::ResourceRequest& resource_request,
    ReadResponseCallback callback) {}
void MockIsolatedWebAppResponseReader::Close(base::OnceClosure callback) {
  std::move(callback).Run();
}

FakeResponseReaderFactory::FakeResponseReaderFactory(
    base::expected<void, UnusableSwbnFileError> bundle_status)
    : IsolatedWebAppResponseReaderFactory(
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
    bool skip_signature_verification,
    Callback callback) {
  // Signatures _must_ be verified during installation.
  CHECK(!skip_signature_verification);
  if (!bundle_status_.has_value()) {
    std::move(callback).Run(base::unexpected(bundle_status_.error()));
  } else {
    std::move(callback).Run(
        std::make_unique<MockIsolatedWebAppResponseReader>());
  }
}

}  // namespace web_app
