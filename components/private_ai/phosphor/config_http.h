// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PRIVATE_AI_PHOSPHOR_CONFIG_HTTP_H_
#define COMPONENTS_PRIVATE_AI_PHOSPHOR_CONFIG_HTTP_H_

#include <memory>
#include <optional>
#include <string>
#include <string_view>

#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "net/third_party/quiche/src/quiche/blind_sign_auth/blind_sign_message_interface.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "url/gurl.h"

namespace network {
class SimpleURLLoader;
}  // namespace network

namespace private_ai::phosphor {

// HTTP Fetching for PrivateAI. This implements the
// `BlindSignMessageInterface` for use by the Blind-Sign-Auth (BSA) library.
class ConfigHttp : public quiche::BlindSignMessageInterface {
 public:
  explicit ConfigHttp(std::unique_ptr<network::PendingSharedURLLoaderFactory>
                          pending_url_loader_factory);
  ~ConfigHttp() override;

  // quiche::BlindSignMessageInterface implementation:
  void DoRequest(quiche::BlindSignMessageRequestType request_type,
                 std::optional<std::string_view> authorization_header,
                 const std::string& body,
                 quiche::BlindSignMessageCallback callback) override;

  static GURL GetServerUrl();
  static std::string GetInitialDataPath();
  static std::string GetTokensPath();

 private:
  network::SharedURLLoaderFactory* GetOrCreateURLLoaderFactory();

  void OnDoRequestCompleted(
      base::TimeTicks start_time,
      std::unique_ptr<network::SimpleURLLoader> url_loader,
      quiche::BlindSignMessageCallback callback,
      std::optional<std::string> response);

  std::unique_ptr<network::PendingSharedURLLoaderFactory>
      pending_url_loader_factory_;

  // Must be created and destroyed in the same sequence, therefore we postpone
  // initialization and do not perform it in the ctor.
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;

  base::WeakPtrFactory<ConfigHttp> weak_ptr_factory_{this};
};

}  // namespace private_ai::phosphor

#endif  // COMPONENTS_PRIVATE_AI_PHOSPHOR_CONFIG_HTTP_H_
