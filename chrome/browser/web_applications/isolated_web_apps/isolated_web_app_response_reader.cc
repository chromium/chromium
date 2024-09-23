// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_response_reader.h"

#include <memory>

#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/memory/weak_ptr.h"
#include "base/types/expected.h"
#include "base/types/expected_macros.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_trust_checker.h"
#include "chrome/browser/web_applications/isolated_web_apps/signed_web_bundle_reader.h"
#include "components/web_package/mojom/web_bundle_parser.mojom-forward.h"
#include "components/web_package/signed_web_bundles/signed_web_bundle_id.h"
#include "services/network/public/cpp/resource_request.h"
#include "url/gurl.h"

namespace web_app {

namespace {

network::ResourceRequest RemoveQuery(
    network::ResourceRequest resource_request) {
  GURL::Replacements replacements;
  replacements.ClearQuery();
  resource_request.url = resource_request.url.ReplaceComponents(replacements);
  return resource_request;
}

}  // namespace

IsolatedWebAppResponseReaderImpl::IsolatedWebAppResponseReaderImpl(
    std::unique_ptr<SignedWebBundleReader> reader,
    TrustChecker trust_checker)
    : reader_(std::move(reader)), trust_checker_(std::move(trust_checker)) {
  CHECK_EQ(reader_->GetState(), SignedWebBundleReader::State::kInitialized);
}

IsolatedWebAppResponseReaderImpl::~IsolatedWebAppResponseReaderImpl() = default;

web_package::SignedWebBundleIntegrityBlock
IsolatedWebAppResponseReaderImpl::GetIntegrityBlock() {
  return reader_->GetIntegrityBlock();
}

void IsolatedWebAppResponseReaderImpl::ReadResponse(
    const network::ResourceRequest& resource_request,
    ReadResponseCallback callback) {
  RETURN_IF_ERROR(Error::FromTrustCheckerResult(trust_checker_.Run()),
                  [&callback](Error error) {
                    std::move(callback).Run(base::unexpected(std::move(error)));
                  });

  // Remove query parameters from the request URL, if it has any. Resources
  // within Signed Web Bundles used for Isolated Web Apps never have username,
  // password, or fragment, just like resources within Signed Web Bundles and
  // normal Web Bundles. Removing these from request URLs is done by the
  // `SignedWebBundleReader`. However, in addition, resources in Signed Web
  // Bundles used for Isolated Web Apps can also never have query parameters,
  // which we need to remove here.
  //
  // Conceptually, we treat the resources in Signed Web Bundles for Isolated Web
  // Apps more like files served by a file server (which also strips query
  // parameters before looking up the file), and not like HTTP exchanges like
  // they are used for Signed Exchanges (SXG).
  reader_->ReadResponse(
      resource_request.url.has_query() ? RemoveQuery(resource_request)
                                       : resource_request,
      base::BindOnce(&IsolatedWebAppResponseReaderImpl::OnResponseRead,
                     // `base::Unretained` is safe to use here, since `this`
                     // owns `reader_`.
                     base::Unretained(this), std::move(callback)));
}

void IsolatedWebAppResponseReaderImpl::OnResponseRead(
    ReadResponseCallback callback,
    base::expected<web_package::mojom::BundleResponsePtr,
                   SignedWebBundleReader::ReadResponseError> response_head) {
  std::move(callback).Run(
      std::move(response_head)
          .transform([this](web_package::mojom::BundleResponsePtr ptr) {
            // Since `this` owns `reader_`, we only pass a weak pointer to it to
            // the `Response` object. If `this` is deleted, it makes sense that
            // the pointer to the `reader_` contained in `Response` also becomes
            // invalid.
            return Response(std::move(ptr), reader_->AsWeakPtr());
          })
          .transform_error(&Error::FromSignedWebBundleReaderError));
}

void IsolatedWebAppResponseReaderImpl::Close(base::OnceClosure callback) {
  reader_->Close(base::BindOnce(&IsolatedWebAppResponseReaderImpl::OnClosed,
                                base::Unretained(this), std::move(callback)));
}

void IsolatedWebAppResponseReaderImpl::OnClosed(base::OnceClosure callback) {
  std::move(callback).Run();
}

IsolatedWebAppResponseReader::Response::Response(
    web_package::mojom::BundleResponsePtr head,
    base::WeakPtr<SignedWebBundleReader> reader)
    : head_(std::move(head)), reader_(std::move(reader)) {}

IsolatedWebAppResponseReader::Response::Response(Response&&) = default;

IsolatedWebAppResponseReader::Response&
IsolatedWebAppResponseReader::Response::operator=(Response&&) = default;

IsolatedWebAppResponseReader::Response::~Response() = default;

void IsolatedWebAppResponseReader::Response::ReadBody(
    mojo::ScopedDataPipeProducerHandle producer_handle,
    base::OnceCallback<void(net::Error net_error)> callback) {
  if (!reader_ ||
      reader_->GetState() != SignedWebBundleReader::State::kInitialized) {
    // The weak pointer to `reader_` might no longer be valid when this is
    // called. Also the reader might be closed.
    std::move(callback).Run(net::ERR_FAILED);
    return;
  }
  reader_->ReadResponseBody(head_->Clone(), std::move(producer_handle),
                            std::move(callback));
}

// static
IsolatedWebAppResponseReader::Error
IsolatedWebAppResponseReader::Error::FromSignedWebBundleReaderError(
    const SignedWebBundleReader::ReadResponseError& error) {
  using Type = SignedWebBundleReader::ReadResponseError::Type;
  switch (error.type) {
    case Type::kFormatError:
      return Error(Error::Type::kFormatError, error.message);
    case Type::kParserInternalError:
      return Error(Error::Type::kParserInternalError, error.message);
    case Type::kResponseNotFound:
      return Error(Error::Type::kResponseNotFound, error.message);
  }
}

// static
base::expected<void, IsolatedWebAppResponseReader::Error>
IsolatedWebAppResponseReader::Error::FromTrustCheckerResult(
    const IsolatedWebAppTrustChecker::Result& result) {
  using Status = IsolatedWebAppTrustChecker::Result::Status;
  switch (result.status) {
    case Status::kTrusted:
      return base::ok();
    case Status::kErrorPublicKeysNotTrusted:
    case Status::kErrorUnsupportedWebBundleIdType:
      return base::unexpected(Error(Error::Type::kNotTrusted, result.message));
  }
}

}  // namespace web_app
