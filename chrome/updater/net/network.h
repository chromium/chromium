// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_NET_NETWORK_H_
#define CHROME_UPDATER_NET_NETWORK_H_

#include <memory>
#include <optional>

#include "base/memory/scoped_refptr.h"
#include "base/sequence_checker.h"
#include "build/build_config.h"
#include "chrome/updater/event_logger.h"
#include "chrome/updater/policy/service.h"
#include "components/update_client/network.h"

namespace updater {

// Creates instances of `NetworkFetcher`. Because of idiosyncrasies of how
// the Windows implementation works, the instance of the factory class must
// outlive the lives of the network fetchers it creates.
class NetworkFetcherFactory : public update_client::NetworkFetcherFactory {
 public:
  NetworkFetcherFactory(std::optional<PolicyServiceProxyConfiguration>
                            policy_service_proxy_configuration,
                        scoped_refptr<UpdaterEventLogger> event_logger);
  NetworkFetcherFactory(const NetworkFetcherFactory&) = delete;
  NetworkFetcherFactory& operator=(const NetworkFetcherFactory&) = delete;

  std::unique_ptr<update_client::NetworkFetcher> Create() const override;

 protected:
  ~NetworkFetcherFactory() override;

 private:
  class Impl;

  SEQUENCE_CHECKER(sequence_checker_);
  std::unique_ptr<Impl> impl_;
};

// Wraps a NetworkFetcher to provide event history logging.
class LoggingNetworkFetcher final : public update_client::NetworkFetcher {
 public:
  explicit LoggingNetworkFetcher(
      std::unique_ptr<update_client::NetworkFetcher> impl);
  ~LoggingNetworkFetcher() override;

  void PostRequest(
      const GURL& url,
      const std::string& post_data,
      const std::string& content_type,
      const base::flat_map<std::string, std::string>& post_additional_headers,
      ResponseStartedCallback response_started_callback,
      ProgressCallback progress_callback,
      PostRequestCompleteCallback post_request_complete_callback) override;

  base::OnceClosure DownloadToFile(
      const GURL& url,
      const base::FilePath& file_path,
      ResponseStartedCallback response_started_callback,
      ProgressCallback progress_callback,
      DownloadToFileCompleteCallback download_to_file_complete_callback)
      override;

 private:
  std::unique_ptr<update_client::NetworkFetcher> impl_;
};

}  // namespace updater

#endif  // CHROME_UPDATER_NET_NETWORK_H_
