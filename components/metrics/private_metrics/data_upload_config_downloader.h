// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_METRICS_PRIVATE_METRICS_DATA_UPLOAD_CONFIG_DOWNLOADER_H_
#define COMPONENTS_METRICS_PRIVATE_METRICS_DATA_UPLOAD_CONFIG_DOWNLOADER_H_

#include <optional>
#include <string>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "third_party/federated_compute/src/fcp/protos/confidentialcompute/data_upload_config.pb.h"

namespace metrics::private_metrics {

// Implementation for downloading a serialized protocol buffer of
// third_party/federated_compute DataUploadConfig from Gstatic for use in
// private metrics.
//
// The DataUploadConfig contains the public key required to
// encrypt private metric reports and the signed endorsements of the keys
// required to perform attestation verification.
//
// This class must only be used when Chrome is running with usage statistics and
// 'Make searches and browsing better' settings enabled, as mentioned in
// `kPrivateMetricsKeyNetworkTag`. However, this class is not responsible for
// managing the logic for the trigger. It is the responsibility of the caller to
// ensure this.
class DataUploadConfigDownloader {
 public:
  explicit DataUploadConfigDownloader(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory);

  DataUploadConfigDownloader(const DataUploadConfigDownloader&) = delete;
  DataUploadConfigDownloader& operator=(const DataUploadConfigDownloader&) =
      delete;

  ~DataUploadConfigDownloader();

  using DataUploadConfigCallback = base::OnceCallback<void(
      std::optional<fcp::confidentialcompute::DataUploadConfig>)>;

  // Initiates an async download of a serialized DataUploadConfig proto from
  // Gstatic. The provided `callback` will be invoked upon completion, either
  // with the downloaded config or an empty optional if the download fails or
  // config is invalid. If `url_loader_factory_` is nullptr, this method will be
  // no-op. This will be the case in unittests.
  void FetchDataUploadConfig(DataUploadConfigCallback callback);

  // Returns `pending_request_`.
  raw_ptr<network::SimpleURLLoader> GetPendingRequestForTesting();

 private:
  // Handles the raw response returned by Gstatic after the download is
  // complete. If `response_body` is a valid DataUploadConfig, `callback` is
  // invoked with a valid DataUploadConfig. Otherwise, `callback` is invoked
  // with std::nullopt. If `pending_request_` is a nullptr, then this function
  // will return without calling the callback. This is a safety check and should
  // not happen under normal circumstances.
  void HandleSerializedDataUploadConfig(
      DataUploadConfigCallback callback,
      std::optional<std::string> response_body);

  std::unique_ptr<network::SimpleURLLoader> pending_request_;

  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;

  // Weak pointers factory used to post task on different threads. All weak
  // pointers managed by this factory have the same lifetime as
  // DataUploadConfigDownloader.
  base::WeakPtrFactory<DataUploadConfigDownloader> self_ptr_factory_{this};
};

}  // namespace metrics::private_metrics

#endif  // COMPONENTS_METRICS_PRIVATE_METRICS_DATA_UPLOAD_CONFIG_DOWNLOADER_H_
