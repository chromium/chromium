// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_IP_PROTECTION_COMMON_IP_PROTECTION_CONFIG_HTTP_H_
#define COMPONENTS_IP_PROTECTION_COMMON_IP_PROTECTION_CONFIG_HTTP_H_

#include <optional>
#include <string>
#include <string_view>

#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/types/expected.h"
#include "net/third_party/quiche/src/quiche/blind_sign_auth/blind_sign_message_interface.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "url/gurl.h"

namespace network {
class SimpleURLLoader;
}  // namespace network

namespace ip_protection {

// HTTP Fetching for IP Protection. This implements the
// `BlindSignMessageInterface` for use by the BSA library.
class IpProtectionConfigHttp : public quiche::BlindSignMessageInterface {
 public:
  explicit IpProtectionConfigHttp(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory);
  ~IpProtectionConfigHttp() override;

  // quiche::BlindSignMessageInterface implementation:
  void DoRequest(quiche::BlindSignMessageRequestType request_type,
                 std::optional<std::string_view> authorization_header,
                 const std::string& body,
                 quiche::BlindSignMessageCallback callback) override;

 private:
  void OnDoRequestCompleted(
      std::unique_ptr<network::SimpleURLLoader> url_loader,
      quiche::BlindSignMessageCallback callback,
      std::unique_ptr<std::string> response);
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;

  const GURL ip_protection_server_url_;
  const std::string ip_protection_server_get_initial_data_path_;
  const std::string ip_protection_server_get_tokens_path_;

  base::WeakPtrFactory<IpProtectionConfigHttp> weak_ptr_factory_{this};
};

}  // namespace ip_protection

#endif  // COMPONENTS_IP_PROTECTION_COMMON_IP_PROTECTION_CONFIG_HTTP_H_
