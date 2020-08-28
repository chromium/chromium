// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/bloom/bloom_interaction.h"

#include "base/bind.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/components/bloom/bloom_controller_impl.h"
#include "chromeos/components/bloom/bloom_interaction.h"
#include "chromeos/components/bloom/public/cpp/future_value.h"
#include "chromeos/components/bloom/screenshot_grabber.h"
#include "chromeos/services/assistant/public/shared/constants.h"
#include "components/signin/public/identity_manager/access_token_info.h"
#include "components/signin/public/identity_manager/primary_account_access_token_fetcher.h"
#include "components/signin/public/identity_manager/scope_set.h"

namespace chromeos {
namespace bloom {

BloomInteraction::BloomInteraction(BloomControllerImpl* controller)
    : controller_(controller), weak_ptr_factory_(this) {}

BloomInteraction::~BloomInteraction() = default;

void BloomInteraction::Start() {
  FetchAccessTokenAsync();
  FetchScreenshotAsync();

  WhenBothAreReady(access_token_future_.get(), screenshot_future_.get(),
                   Bind(&BloomInteraction::StartAssistantInteraction));
}

void BloomInteraction::StartAssistantInteraction(std::string&& access_token,
                                                 Screenshot&& screenshot) {
  controller_->ShowUI();
  // TODO(jeroendh): continue here by contacting the Bloom service.
}

void BloomInteraction::FetchAccessTokenAsync() {
  signin::ScopeSet scopes;
  scopes.insert(assistant::kBloomScope);

  auto mode =
      signin::PrimaryAccountAccessTokenFetcher::Mode::kWaitUntilAvailable;

  access_token_fetcher_ =
      std::make_unique<signin::PrimaryAccountAccessTokenFetcher>(
          /*consumer_name=*/"BloomAccessTokenFetcher",
          controller_->identity_manager(), scopes,
          Bind(&BloomInteraction::OnAccessTokenRequestCompleted), mode);
  access_token_future_ = std::make_unique<AccessTokenFuture>();
}

void BloomInteraction::FetchScreenshotAsync() {
  controller_->screenshot_grabber()->TakeScreenshot(
      Bind(&BloomInteraction::OnScreenshotReady));
  screenshot_future_ = std::make_unique<ScreenshotFuture>();
}

void BloomInteraction::OnAccessTokenRequestCompleted(
    GoogleServiceAuthError error,
    signin::AccessTokenInfo access_token_info) {
  if (error.state() != GoogleServiceAuthError::NONE) {
    controller_->StopInteraction(BloomInteractionResolution::kNoAccessToken);
    return;
  }

  access_token_future_->SetValue(std::move(access_token_info.token));
}

void BloomInteraction::OnScreenshotReady(
    base::Optional<Screenshot> screenshot) {
  if (!screenshot) {
    controller_->StopInteraction(BloomInteractionResolution::kNoScreenshot);
    return;
  }

  screenshot_future_->SetValue(std::move(screenshot.value()));
}

}  // namespace bloom
}  // namespace chromeos
