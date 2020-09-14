// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_BLOOM_BLOOM_INTERACTION_H_
#define CHROMEOS_COMPONENTS_BLOOM_BLOOM_INTERACTION_H_

#include <string>
#include "base/bind.h"
#include "base/callback_forward.h"
#include "base/optional.h"

class GoogleServiceAuthError;

namespace gfx {
class Image;
}

namespace signin {
struct AccessTokenInfo;
class PrimaryAccountAccessTokenFetcher;
}  // namespace signin

namespace chromeos {
namespace bloom {

class BloomControllerImpl;

template <typename _Type>
class FutureValue;
using AccessTokenFuture = FutureValue<std::string>;
using ScreenshotFuture = FutureValue<gfx::Image>;

// A single Bloom interaction. This will:
//    * Fetch the access token and screenshot.
//    * Start the Assistant interaction (and open the UI).
//    * Fetch the Bloom response and forward it to the Assistant interaction.
class BloomInteraction {
  using StartCallback = base::OnceCallback<void(const std::string& access_token,
                                                gfx::Image&& screenshot)>;

 public:
  explicit BloomInteraction(BloomControllerImpl* controller);

  BloomInteraction(const BloomInteraction&) = delete;
  BloomInteraction& operator=(const BloomInteraction&) = delete;
  ~BloomInteraction();

  void Start();

 private:
  void StartAssistantInteraction(std::string&& access_token,
                                 gfx::Image&& screenshot);

  void OnServerResponse(base::Optional<std::string> html);

  void FetchAccessTokenAsync();
  void FetchScreenshotAsync();

  void OnAccessTokenRequestCompleted(GoogleServiceAuthError error,
                                     signin::AccessTokenInfo access_token_info);
  void OnScreenshotReady(base::Optional<gfx::Image> screenshot);

  template <typename _Method, typename... Args>
  auto Bind(_Method method, Args&&... args) {
    return base::BindOnce(method, weak_ptr_factory_.GetWeakPtr(),
                          std::forward<Args>(args)...);
  }

  BloomControllerImpl* const controller_;

  std::unique_ptr<signin::PrimaryAccountAccessTokenFetcher>
      access_token_fetcher_;
  std::unique_ptr<AccessTokenFuture> access_token_future_;
  std::unique_ptr<ScreenshotFuture> screenshot_future_;

  base::WeakPtrFactory<BloomInteraction> weak_ptr_factory_;
};

}  // namespace bloom
}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_BLOOM_BLOOM_INTERACTION_H_
