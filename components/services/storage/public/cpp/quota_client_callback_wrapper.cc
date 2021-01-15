// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/storage/public/cpp/quota_client_callback_wrapper.h"

#include <utility>

#include "base/check_op.h"
#include "base/sequence_checker.h"
#include "components/services/storage/public/mojom/quota_client.mojom.h"
#include "mojo/public/cpp/bindings/callback_helpers.h"
#include "third_party/blink/public/mojom/quota/quota_types.mojom.h"

namespace storage {

QuotaClientCallbackWrapper::QuotaClientCallbackWrapper(
    mojom::QuotaClient* wrapped_client)
    : wrapped_client_(wrapped_client) {
  DCHECK(wrapped_client);
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

QuotaClientCallbackWrapper::~QuotaClientCallbackWrapper() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void QuotaClientCallbackWrapper::GetOriginUsage(
    const url::Origin& origin,
    blink::mojom::StorageType type,
    GetOriginUsageCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  wrapped_client_->GetOriginUsage(
      origin, type,
      mojo::WrapCallbackWithDefaultInvokeIfNotRun(std::move(callback), 0));
}

void QuotaClientCallbackWrapper::GetOriginsForType(
    blink::mojom::StorageType type,
    GetOriginsForTypeCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  wrapped_client_->GetOriginsForType(
      type, mojo::WrapCallbackWithDefaultInvokeIfNotRun(
                std::move(callback), std::vector<url::Origin>()));
}

void QuotaClientCallbackWrapper::GetOriginsForHost(
    blink::mojom::StorageType type,
    const std::string& host,
    GetOriginsForHostCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  wrapped_client_->GetOriginsForHost(
      type, host,
      mojo::WrapCallbackWithDefaultInvokeIfNotRun(std::move(callback),
                                                  std::vector<url::Origin>()));
}

void QuotaClientCallbackWrapper::DeleteOriginData(
    const url::Origin& origin,
    blink::mojom::StorageType type,
    DeleteOriginDataCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  wrapped_client_->DeleteOriginData(
      origin, type,
      mojo::WrapCallbackWithDefaultInvokeIfNotRun(
          std::move(callback), blink::mojom::QuotaStatusCode::kErrorAbort));
}

void QuotaClientCallbackWrapper::PerformStorageCleanup(
    blink::mojom::StorageType type,
    PerformStorageCleanupCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  wrapped_client_->PerformStorageCleanup(
      type, mojo::WrapCallbackWithDefaultInvokeIfNotRun(std::move(callback)));
}

}  // namespace storage
