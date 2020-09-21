// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/bloom/bloom_interaction.h"

#include "base/bind.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/components/bloom/bloom_controller_impl.h"
#include "chromeos/components/bloom/bloom_interaction.h"
#include "chromeos/components/bloom/public/cpp/bloom_screenshot_delegate.h"
#include "chromeos/components/bloom/public/cpp/future_value.h"
#include "chromeos/components/bloom/server/bloom_server_proxy.h"
#include "chromeos/services/assistant/public/shared/constants.h"
#include "components/signin/public/identity_manager/access_token_info.h"
#include "components/signin/public/identity_manager/primary_account_access_token_fetcher.h"
#include "components/signin/public/identity_manager/scope_set.h"
#include "ui/gfx/image/image.h"

namespace chromeos {
namespace bloom {

BloomInteraction::BloomInteraction(BloomControllerImpl* controller)
    : controller_(controller), weak_ptr_factory_(this) {}

BloomInteraction::~BloomInteraction() {
  // TODO(jeroendh): Cancel ongoing processes here, like the screenshot fetcher
  // and the bloom server requests.
}

void BloomInteraction::Start() {
  FetchAccessTokenAsync();
  FetchScreenshotAsync();

  WhenBothAreReady(access_token_future_.get(), screenshot_future_.get(),
                   Bind(&BloomInteraction::StartAssistantInteraction));
}

void BloomInteraction::StartAssistantInteraction(std::string&& access_token,
                                                 gfx::Image&& screenshot) {
  DVLOG(2) << "Opening assistant UI";
  controller_->ShowUI();
  DVLOG(2) << "Contacting Bloom server";
  controller_->server_proxy()->AnalyzeProblem(
      access_token, std::move(screenshot),
      Bind(&BloomInteraction::OnServerResponse));
}

void BloomInteraction::OnServerResponse(base::Optional<std::string> html) {
  if (!html) {
    controller_->StopInteraction(BloomInteractionResolution ::kServerError);
    return;
  }

  DVLOG(2) << "Got server response";
  controller_->ShowResult(html.value());
}

void BloomInteraction::FetchAccessTokenAsync() {
  DVLOG(2) << "Fetching access token";

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
  controller_->screenshot_delegate()->TakeScreenshot(
      Bind(&BloomInteraction::OnScreenshotReady));
  screenshot_future_ = std::make_unique<ScreenshotFuture>();
}

void BloomInteraction::OnAccessTokenRequestCompleted(
    GoogleServiceAuthError error,
    signin::AccessTokenInfo access_token_info) {
  if (error.state() != GoogleServiceAuthError::NONE) {
    LOG(WARNING) << "Failed to fetch the access token";
    controller_->StopInteraction(BloomInteractionResolution::kNoAccessToken);
    return;
  }

  DVLOG(2) << "Received access token";
  access_token_future_->SetValue(std::move(access_token_info.token));
}

void BloomInteraction::OnScreenshotReady(
    base::Optional<gfx::Image> screenshot) {
  if (!screenshot) {
    LOG(WARNING) << "Failed to take the screenshot";
    controller_->StopInteraction(BloomInteractionResolution::kNoScreenshot);
    return;
  }

  DVLOG(2) << "Received screenshot";
  screenshot_future_->SetValue(std::move(screenshot.value()));
}

}  // namespace bloom
}  // namespace chromeos
