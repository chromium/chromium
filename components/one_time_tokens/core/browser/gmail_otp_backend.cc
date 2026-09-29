// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/one_time_tokens/core/browser/gmail_otp_backend.h"

#include <memory>
#include <utility>

#include "base/memory/scoped_refptr.h"
#include "components/one_time_tokens/core/browser/gmail_otp_backend_impl.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace signin {
class IdentityManager;
}  // namespace signin

namespace one_time_tokens {

GmailOtpBackend::~GmailOtpBackend() = default;

// static
std::unique_ptr<GmailOtpBackend> GmailOtpBackend::Create(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    signin::IdentityManager& identity_manager) {
  return std::make_unique<GmailOtpBackendImpl>(std::move(url_loader_factory),
                                               identity_manager);
}

}  // namespace one_time_tokens
