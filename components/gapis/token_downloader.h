// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_GAPIS_TOKEN_DOWNLOADER_H_
#define COMPONENTS_GAPIS_TOKEN_DOWNLOADER_H_

#include <memory>
#include <optional>
#include <string>

#include "base/feature_list.h"
#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/timer/timer.h"
#include "url/gurl.h"

namespace network {
class SharedURLLoaderFactory;
class SimpleURLLoader;
}  // namespace network

namespace gapis {

BASE_DECLARE_FEATURE(kEnableGapis);

class TokenDownloader {
 public:
  using FetchTokenCallback = base::OnceCallback<void(const std::string&)>;

  TokenDownloader(
      const GURL& gapis_service_url,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory);
  TokenDownloader(const TokenDownloader&) = delete;
  TokenDownloader& operator=(const TokenDownloader&) = delete;
  ~TokenDownloader();

  // Fetches the token from the server. The callback will be run with the token
  // if successful, or an empty string.
  void FetchToken(FetchTokenCallback callback,
                  const std::string& access_token,
                  const std::string& signed_challenge);

 private:
  void OnSimpleLoaderComplete(std::optional<std::string> response_body);
  void OnTimeout();

  FetchTokenCallback fetch_token_callback_;

  // The URL for the obtain token RPC.
  const GURL obtain_token_url_;

  // The URL loader for the network request.
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;

  // The current URL loader. Null unless a request is in progress.
  std::unique_ptr<network::SimpleURLLoader> simple_url_loader_;

  // Handles timing out requests.
  base::OneShotTimer timer_;
};

}  // namespace gapis

#endif  // COMPONENTS_GAPIS_TOKEN_DOWNLOADER_H_
