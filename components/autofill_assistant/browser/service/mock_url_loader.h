// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_SERVICE_MOCK_URL_LOADER_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_SERVICE_MOCK_URL_LOADER_H_

#include "components/autofill_assistant/browser/service/service_request_sender.h"
#include "services/network/public/cpp/simple_url_loader.h"

#include "testing/gmock/include/gmock/gmock.h"

namespace network {
class SimpleURLLoaderThrottle;
namespace mojom {
class URLLoaderFactory;
}  // namespace mojom
}  // namespace network

namespace autofill_assistant {

// TODO(arbesser): Remove this once we pass in the mojom interface to our
// service, instead of the SimpleURLLoader.
class MockURLLoader : public ::network::SimpleURLLoader {
 public:
  MockURLLoader();
  ~MockURLLoader() override;
  MOCK_METHOD3(DownloadToString,
               void(::network::mojom::URLLoaderFactory* url_loader_factory,
                    BodyAsStringCallback body_as_string_callback,
                    size_t max_body_size));
  MOCK_METHOD2(DownloadToStringOfUnboundedSizeUntilCrashAndDie,
               void(::network::mojom::URLLoaderFactory* url_loader_factory,
                    BodyAsStringCallback body_as_string_callback));
  MOCK_METHOD2(DownloadHeadersOnly,
               void(::network::mojom::URLLoaderFactory* url_loader_factory,
                    HeadersOnlyCallback headers_only_callback));
  MOCK_METHOD4(
      DownloadToFile,
      void(::network::mojom::URLLoaderFactory* url_loader_factory,
           DownloadToFileCompleteCallback download_to_file_complete_callback,
           const base::FilePath& file_path,
           int64_t max_body_size));
  MOCK_METHOD3(
      DownloadToTempFile,
      void(::network::mojom::URLLoaderFactory* url_loader_factory,
           DownloadToFileCompleteCallback download_to_file_complete_callback,
           int64_t max_body_size));
  MOCK_METHOD2(DownloadAsStream,
               void(::network::mojom::URLLoaderFactory* url_loader_factory,
                    ::network::SimpleURLLoaderStreamConsumer* stream_consumer));
  MOCK_METHOD1(SetOnRedirectCallback,
               void(const OnRedirectCallback& on_redirect_callback));
  MOCK_METHOD1(SetOnResponseStartedCallback,
               void(OnResponseStartedCallback on_response_started_callback));
  MOCK_METHOD1(SetOnUploadProgressCallback,
               void(UploadProgressCallback on_upload_progress_callback));
  MOCK_METHOD1(SetOnDownloadProgressCallback,
               void(DownloadProgressCallback on_download_progress_callback));
  MOCK_METHOD1(SetAllowPartialResults, void(bool allow_partial_results));
  MOCK_METHOD1(SetAllowHttpErrorResults, void(bool allow_http_error_results));
  MOCK_METHOD2(AttachStringForUpload,
               void(const std::string& upload_data,
                    const std::string& upload_content_type));
  MOCK_METHOD4(AttachFileForUpload,
               void(const base::FilePath& upload_file_path,
                    const std::string& upload_content_type,
                    uint64_t offset,
                    uint64_t length));
  MOCK_METHOD2(SetRetryOptions, void(int max_retries, int retry_mode));
  MOCK_METHOD1(SetURLLoaderFactoryOptions, void(uint32_t options));
  MOCK_METHOD1(SetRequestID, void(int32_t request_id));
  MOCK_METHOD1(SetTimeoutDuration, void(base::TimeDelta timeout_duration));
  MOCK_METHOD0(SetAllowBatching, void());
  MOCK_CONST_METHOD0(NetError, int());
  MOCK_CONST_METHOD0(ResponseInfo, const ::network::mojom::URLResponseHead*());
  MOCK_CONST_METHOD0(CompletionStatus,
                     absl::optional<::network::URLLoaderCompletionStatus>&());
  MOCK_CONST_METHOD0(GetFinalURL, const GURL&());
  MOCK_CONST_METHOD0(LoadedFromCache, bool());
  MOCK_CONST_METHOD0(GetContentSize, int64_t());
  MOCK_CONST_METHOD0(GetNumRetries, int());
  MOCK_METHOD0(GetThrottleForTesting, ::network::SimpleURLLoaderThrottle*());
};

}  // namespace autofill_assistant

#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_SERVICE_MOCK_URL_LOADER_H_
