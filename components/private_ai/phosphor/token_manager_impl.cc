// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/private_ai/phosphor/token_manager_impl.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "base/check.h"
#include "components/private_ai/common/private_ai_logger.h"
#include "components/private_ai/features.h"
#include "components/private_ai/phosphor/feature_token_manager.h"
#include "components/private_ai/phosphor/token_fetcher.h"
#include "net/third_party/quiche/src/quiche/blind_sign_auth/blind_sign_auth_interface.h"

namespace private_ai::phosphor {

TokenManagerImpl::TokenManagerImpl(std::unique_ptr<TokenFetcher> fetcher,
                                   PrivateAiLogger* logger)
    : batch_size_(kPrivateAiAuthTokenCacheBatchSize.Get()),
      cache_low_water_mark_(kPrivateAiAuthTokenCacheLowWaterMark.Get()),
      logger_(logger),
      fetcher_(std::move(fetcher)) {
  CHECK(logger_);
  terminal_token_manager_ = std::make_unique<internal::FeatureTokenManager>(
      fetcher_.get(), logger_, quiche::ProxyLayer::kTerminalLayer, batch_size_,
      cache_low_water_mark_);
  proxy_token_manager_ = std::make_unique<internal::FeatureTokenManager>(
      fetcher_.get(), logger_, quiche::ProxyLayer::kProxyB, batch_size_,
      cache_low_water_mark_);
}

TokenManagerImpl::~TokenManagerImpl() = default;

void TokenManagerImpl::GetAuthToken(GetAuthTokenCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  terminal_token_manager_->GetAuthToken(std::move(callback));
}

void TokenManagerImpl::PrefetchAuthTokens() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  terminal_token_manager_->PrefetchAuthTokens();
}

void TokenManagerImpl::GetAuthTokenForProxy(GetAuthTokenCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  proxy_token_manager_->GetAuthToken(std::move(callback));
}

void TokenManagerImpl::PrefetchAuthTokensForProxy() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  proxy_token_manager_->PrefetchAuthTokens();
}

void TokenManagerImpl::OnAccountStatusChanged(bool available) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  terminal_token_manager_->OnAccountStatusChanged(available);
  proxy_token_manager_->OnAccountStatusChanged(available);
}

PrivateAiLogger* TokenManagerImpl::GetLogger() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return logger_;
}

}  // namespace private_ai::phosphor
