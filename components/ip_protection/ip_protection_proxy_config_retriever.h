// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_IP_PROTECTION_IP_PROTECTION_PROXY_CONFIG_RETRIEVER_H_
#define COMPONENTS_IP_PROTECTION_IP_PROTECTION_PROXY_CONFIG_RETRIEVER_H_

#include <optional>
#include <string>

#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "base/types/expected.h"
#include "components/ip_protection/get_proxy_config.pb.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "url/gurl.h"

namespace network {
class SimpleURLLoader;
}  // namespace network

namespace ip_protection {

// Retrieves proxy configuration that is necessary for IP Protection from the
// server.
class IpProtectionProxyConfigRetriever {
 public:
  explicit IpProtectionProxyConfigRetriever(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      std::string type,
      std::string api_key);
  virtual ~IpProtectionProxyConfigRetriever();
  using GetProxyConfigCallback = base::OnceCallback<void(
      base::expected<ip_protection::GetProxyConfigResponse, std::string>)>;
  virtual void GetProxyConfig(std::optional<std::string> oauth_token,
                              GetProxyConfigCallback callback,
                              bool for_testing = false);

 private:
  void OnGetProxyConfigCompleted(
      std::unique_ptr<network::SimpleURLLoader> url_loader,
      GetProxyConfigCallback callback,
      std::unique_ptr<std::string> response);

  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
  const GURL ip_protection_server_url_;
  const std::string ip_protection_server_get_proxy_config_path_;
  const std::string service_type_;
  const std::string api_key_;
  base::WeakPtrFactory<IpProtectionProxyConfigRetriever> weak_ptr_factory_{
      this};
};

}  // namespace ip_protection

#endif  // COMPONENTS_IP_PROTECTION_IP_PROTECTION_PROXY_CONFIG_RETRIEVER_H_
