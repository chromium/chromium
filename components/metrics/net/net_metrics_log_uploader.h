// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_METRICS_NET_NET_METRICS_LOG_UPLOADER_H_
#define COMPONENTS_METRICS_NET_NET_METRICS_LOG_UPLOADER_H_

#include <memory>
#include <string>
#include <string_view>

#include "components/metrics/metrics_log.h"
#include "components/metrics/metrics_log_uploader.h"
#include "third_party/metrics_proto/reporting_info.pb.h"
#include "url/gurl.h"

namespace network {
class SharedURLLoaderFactory;
class SimpleURLLoader;
}  // namespace network

namespace metrics {

// Implementation of MetricsLogUploader using the Chrome network stack.
class NetMetricsLogUploader : public MetricsLogUploader {
 public:
  // Constructs a NetMetricsLogUploader which uploads data to |server_url| with
  // the specified |mime_type|. The |service_type| marks which service the
  // data usage should be attributed to. The |on_upload_complete| callback will
  // be called with the HTTP response code of the upload or with -1 on an error.
  NetMetricsLogUploader(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      const GURL& server_url,
      std::string_view mime_type,
      MetricsLogUploader::MetricServiceType service_type,
      const MetricsLogUploader::UploadCallback& on_upload_complete);

  // This constructor allows a secondary non-HTTPS URL to be passed in as
  // |insecure_server_url|. That URL is used as a fallback if a connection
  // to |server_url| fails, requests are encrypted when sent to an HTTP URL.
  NetMetricsLogUploader(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      const GURL& server_url,
      const GURL& insecure_server_url,
      std::string_view mime_type,
      MetricsLogUploader::MetricServiceType service_type,
      const MetricsLogUploader::UploadCallback& on_upload_complete);

  NetMetricsLogUploader(const NetMetricsLogUploader&) = delete;
  NetMetricsLogUploader& operator=(const NetMetricsLogUploader&) = delete;

  ~NetMetricsLogUploader() override;

  // MetricsLogUploader:
  // Uploads a log to the server_url specified in the constructor.
  void UploadLog(const std::string& compressed_log_data,
                 const LogMetadata& log_metadata,
                 const std::string& log_hash,
                 const std::string& log_signature,
                 const ReportingInfo& reporting_info) override;

 private:
  // Uploads a log to a URL passed as a parameter.
  void UploadLogToURL(const std::string& compressed_log_data,
                      const LogMetadata& log_metadata,
                      const std::string& log_hash,
                      const std::string& log_signature,
                      const ReportingInfo& reporting_info,
                      const GURL& url);

  // Calls |on_upload_complete_| with failure codes. Used when there's a local
  // reason that prevented an upload over HTTP, such as an error encrpyting
  // the payload.
  void HTTPFallbackAborted();

  void OnURLLoadComplete(std::unique_ptr<std::string> response_body);

  // The URLLoader factory for loads done using the network stack.
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;

  const GURL server_url_;
  const GURL insecure_server_url_;
  const std::string mime_type_;
  const MetricsLogUploader ::MetricServiceType service_type_;
  const MetricsLogUploader::UploadCallback on_upload_complete_;
  // The outstanding transmission appears as a URL Fetch operation.
  std::unique_ptr<network::SimpleURLLoader> url_loader_;
};

}  // namespace metrics

#endif  // COMPONENTS_METRICS_NET_NET_METRICS_LOG_UPLOADER_H_
