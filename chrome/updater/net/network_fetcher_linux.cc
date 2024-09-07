// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include <curl/curl.h>
#include <curl/system.h>
#include <dlfcn.h>

#include <array>
#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "base/containers/flat_map.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/numerics/checked_math.h"
#include "base/sequence_checker.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/threading/sequence_bound.h"
#include "chrome/updater/net/network.h"
#include "chrome/updater/policy/service.h"
#include "chrome/updater/util/util.h"
#include "components/update_client/network.h"
#include "url/gurl.h"

namespace updater {
namespace {

struct CurlDeleter {
  void operator()(CURL* curl) { curl_easy_cleanup(curl); }
};
using CurlUniquePtr = std::unique_ptr<CURL, CurlDeleter>;

class LibcurlNetworkFetcherImpl {
 public:
  using ResponseStartedCallback =
      update_client::NetworkFetcher::ResponseStartedCallback;
  using ProgressCallback = update_client::NetworkFetcher::ProgressCallback;
  using PostRequestCompleteCallback =
      update_client::NetworkFetcher::PostRequestCompleteCallback;
  using DownloadToFileCompleteCallback =
      update_client::NetworkFetcher::DownloadToFileCompleteCallback;

  LibcurlNetworkFetcherImpl() = delete;
  LibcurlNetworkFetcherImpl(const LibcurlNetworkFetcherImpl&) = delete;
  LibcurlNetworkFetcherImpl& operator=(const LibcurlNetworkFetcherImpl&) =
      delete;
  ~LibcurlNetworkFetcherImpl();

  LibcurlNetworkFetcherImpl(
      CurlUniquePtr curl,
      scoped_refptr<base::SequencedTaskRunner> callback_sequence_);

  void PostRequest(
      const GURL& url,
      const std::string& post_data,
      const std::string& content_type,
      const base::flat_map<std::string, std::string>& post_additional_headers,
      ResponseStartedCallback response_started_callback,
      ProgressCallback progress_callback,
      PostRequestCompleteCallback post_request_complete_callback);

  void DownloadToFile(
      const GURL& url,
      const base::FilePath& file_path,
      ResponseStartedCallback response_started_callback,
      ProgressCallback progress_callback,
      DownloadToFileCompleteCallback download_to_file_complete_callback);

 private:
  SEQUENCE_CHECKER(sequence_checker_);

  // Static callbacks for libcurl.
  static size_t CurlWriteStringCallback(void* data,
                                        size_t size,
                                        size_t nmemb,
                                        void* userp);
  static size_t CurlHeaderCallback(char* data,
                                   size_t size,
                                   size_t nmemb,
                                   void* userp);
  static size_t CurlWriteFileCallback(void* data,
                                      size_t size,
                                      size_t nmemb,
                                      void* userp);
  static int CurlTransferCallback(void* userp,
                                  curl_off_t dltotal,
                                  curl_off_t dlnow,
                                  curl_off_t ultotal,
                                  curl_off_t ulnow);

  void OnTransferInfo(curl_off_t total, curl_off_t current);

  // Helper function to find the value of a header or return an empty string.
  static std::string GetHeaderValue(
      const base::flat_map<std::string, std::string>& response_headers,
      const std::string& header) {
    const std::string lower = base::ToLowerASCII(header);
    return response_headers.contains(lower) ? response_headers.at(lower) : "";
  }

  CurlUniquePtr curl_;
  std::array<char, CURL_ERROR_SIZE> curl_error_buf_;

  // Sequence to post callbacks to.
  scoped_refptr<base::SequencedTaskRunner> callback_sequence_;

  ResponseStartedCallback response_started_callback_;
  ProgressCallback progress_callback_;

  base::WeakPtrFactory<LibcurlNetworkFetcherImpl> weak_factory_{this};
};

LibcurlNetworkFetcherImpl::LibcurlNetworkFetcherImpl(
    CurlUniquePtr curl,
    scoped_refptr<base::SequencedTaskRunner> callback_sequence)
    : curl_(std::move(curl)), callback_sequence_(callback_sequence) {}
LibcurlNetworkFetcherImpl::~LibcurlNetworkFetcherImpl() = default;

void LibcurlNetworkFetcherImpl::PostRequest(
    const GURL& url,
    const std::string& post_data,
    const std::string& content_type,
    const base::flat_map<std::string, std::string>& post_additional_headers,
    ResponseStartedCallback response_started_callback,
    ProgressCallback progress_callback,
    PostRequestCompleteCallback post_request_complete_callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  VLOG(2) << __func__;

  curl_easy_reset(curl_.get());

  struct curl_slist* headers = nullptr;

  headers =
      curl_slist_append(headers, ("Content-Type: " + content_type).c_str());
  for (const auto& [key, value] : post_additional_headers) {
    headers = curl_slist_append(headers, (key + ": " + value).c_str());
  }

  base::flat_map<std::string, std::string> response_headers;
  std::unique_ptr<std::string> response_body = std::make_unique<std::string>();

  base::WeakPtr<LibcurlNetworkFetcherImpl> weak_ptr =
      weak_factory_.GetWeakPtr();
  if (curl_easy_setopt(curl_.get(), CURLOPT_URL, url.spec().c_str()) ||
      curl_easy_setopt(curl_.get(), CURLOPT_HTTPPOST, 1L) ||
      curl_easy_setopt(curl_.get(), CURLOPT_USERAGENT,
                       GetUpdaterUserAgent().c_str()) ||
      curl_easy_setopt(curl_.get(), CURLOPT_HTTPHEADER, headers) ||
      curl_easy_setopt(curl_.get(), CURLOPT_POSTFIELDSIZE, post_data.size()) ||
      curl_easy_setopt(curl_.get(), CURLOPT_POSTFIELDS, post_data.c_str()) ||
      curl_easy_setopt(curl_.get(), CURLOPT_HEADERFUNCTION,
                       &LibcurlNetworkFetcherImpl::CurlHeaderCallback) ||
      curl_easy_setopt(curl_.get(), CURLOPT_HEADERDATA, &response_headers) ||
      curl_easy_setopt(curl_.get(), CURLOPT_WRITEFUNCTION,
                       &LibcurlNetworkFetcherImpl::CurlWriteStringCallback) ||
      curl_easy_setopt(curl_.get(), CURLOPT_WRITEDATA, response_body.get()) ||
      curl_easy_setopt(curl_.get(), CURLOPT_NOPROGRESS, 0) ||
      curl_easy_setopt(curl_.get(), CURLOPT_XFERINFOFUNCTION,
                       &LibcurlNetworkFetcherImpl::CurlTransferCallback) ||
      curl_easy_setopt(curl_.get(), CURLOPT_XFERINFODATA, &weak_ptr) ||
      curl_easy_setopt(curl_.get(), CURLOPT_ERRORBUFFER,
                       curl_error_buf_.data())) {
    VLOG(1) << "Failed to set curl options for HTTP POST.";
    curl_slist_free_all(headers);
    return;
  }

  response_started_callback_ = std::move(response_started_callback);
  progress_callback_ = std::move(progress_callback);

  CURLcode result = curl_easy_perform(curl_.get());
  if (result != CURLE_OK) {
    VLOG(1) << "Failed to perform HTTP POST. "
            << (curl_error_buf_[0] ? curl_error_buf_.data() : "")
            << " (CURLcode " << result << ")";
  }

  int x_retry_after;
  if (!base::StringToInt(
          GetHeaderValue(response_headers,
                         update_client::NetworkFetcher::kHeaderXRetryAfter),
          &x_retry_after)) {
    x_retry_after = -1;
  }

  callback_sequence_->PostTask(
      FROM_HERE,
      base::BindOnce(
          std::move(post_request_complete_callback), std::move(response_body),
          CURLE_OK,
          GetHeaderValue(response_headers,
                         update_client::NetworkFetcher::kHeaderEtag),
          GetHeaderValue(response_headers,
                         update_client::NetworkFetcher::kHeaderXCupServerProof),
          x_retry_after));

  curl_slist_free_all(headers);
}

void LibcurlNetworkFetcherImpl::DownloadToFile(
    const GURL& url,
    const base::FilePath& file_path,
    ResponseStartedCallback response_started_callback,
    ProgressCallback progress_callback,
    DownloadToFileCompleteCallback download_to_file_complete_callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  VLOG(2) << __func__;

  base::File file;
  file.Initialize(file_path, base::File::Flags::FLAG_CREATE_ALWAYS |
                                 base::File::Flags::FLAG_WRITE);
  if (!file.IsValid()) {
    VLOG(1) << "LibcurlNetworkFetcherImpl cannot open file for download.";
    callback_sequence_->PostTask(
        FROM_HERE, base::BindOnce(std::move(download_to_file_complete_callback),
                                  CURLE_WRITE_ERROR, 0));
    return;
  }

  curl_easy_reset(curl_.get());

  base::WeakPtr<LibcurlNetworkFetcherImpl> weak_ptr =
      weak_factory_.GetWeakPtr();
  if (curl_easy_setopt(curl_.get(), CURLOPT_URL, url.spec().c_str()) ||
      curl_easy_setopt(curl_.get(), CURLOPT_HTTPGET, 1L) ||
      curl_easy_setopt(curl_.get(), CURLOPT_USERAGENT,
                       GetUpdaterUserAgent().c_str()) ||
      curl_easy_setopt(curl_.get(), CURLOPT_WRITEFUNCTION,
                       &LibcurlNetworkFetcherImpl::CurlWriteFileCallback) ||
      curl_easy_setopt(curl_.get(), CURLOPT_WRITEDATA, &file) ||
      curl_easy_setopt(curl_.get(), CURLOPT_NOPROGRESS, 0) ||
      curl_easy_setopt(curl_.get(), CURLOPT_XFERINFOFUNCTION,
                       &LibcurlNetworkFetcherImpl::CurlTransferCallback) ||
      curl_easy_setopt(curl_.get(), CURLOPT_XFERINFODATA, &weak_ptr) ||
      curl_easy_setopt(curl_.get(), CURLOPT_ERRORBUFFER,
                       curl_error_buf_.data())) {
    VLOG(1) << "Failed to set curl options for HTTP GET.";
    return;
  }

  response_started_callback_ = std::move(response_started_callback);
  progress_callback_ = std::move(progress_callback);

  curl_off_t downloaded_bytes = 0;
  curl_error_buf_[0] = '\0';
  CURLcode result = curl_easy_perform(curl_.get());
  if (result != CURLE_OK) {
    VLOG(1) << "Failed to perform HTTP GET. "
            << (curl_error_buf_[0] ? curl_error_buf_.data() : "")
            << " (CURLcode " << result << ")";
  } else if (curl_easy_getinfo(curl_.get(), CURLINFO_SIZE_DOWNLOAD_T,
                               &downloaded_bytes) != CURLE_OK) {
    VLOG(1) << "Cannot retrieve downloaded bytes for finished transfer";
    downloaded_bytes = 0;
  }

  file.Close();
  callback_sequence_->PostTask(
      FROM_HERE, base::BindOnce(std::move(download_to_file_complete_callback),
                                result, downloaded_bytes));
}

void LibcurlNetworkFetcherImpl::OnTransferInfo(curl_off_t total,
                                               curl_off_t current) {
  if (response_started_callback_ && total) {
    // Query for an HTTP response code. If one has not been sent yet, the
    // transfer has not started.
    long response_code = 0;
    if (curl_easy_getinfo(curl_.get(), CURLINFO_RESPONSE_CODE,
                          &response_code) != CURLE_OK) {
      VLOG(1) << "Cannot retrieve HTTP response code for ongoing transfer.";
      return;
    } else if (response_code) {
      callback_sequence_->PostTask(
          FROM_HERE, base::BindOnce(std::move(response_started_callback_),
                                    response_code, total));
    }
  }

  if (progress_callback_ && current) {
    callback_sequence_->PostTask(FROM_HERE,
                                 base::BindOnce(progress_callback_, current));
  }
}

size_t LibcurlNetworkFetcherImpl::CurlWriteStringCallback(void* data,
                                                          size_t member_size,
                                                          size_t num_members,
                                                          void* userp) {
  base::CheckedNumeric<size_t> write_size =
      base::CheckedNumeric<size_t>(member_size) *
      base::CheckedNumeric<size_t>(num_members);
  static_cast<std::string*>(userp)->append(static_cast<const char*>(data),
                                           write_size.ValueOrDefault(0));
  return write_size.ValueOrDefault(0);
}

size_t LibcurlNetworkFetcherImpl::CurlHeaderCallback(char* data,
                                                     size_t member_size,
                                                     size_t num_members,
                                                     void* userp) {
  auto* headers = static_cast<base::flat_map<std::string, std::string>*>(userp);

  base::CheckedNumeric<size_t> buf_size =
      base::CheckedNumeric<size_t>(member_size) *
      base::CheckedNumeric<size_t>(num_members);

  std::string line(data, buf_size.ValueOrDefault(0));
  // Reject any headers that aren't compliant with RFC 5987.
  // Returning 0 will abort the transfer.
  if (!base::IsStringASCII(line)) {
    return 0;
  }

  size_t delim_pos = line.find(":");
  if (delim_pos != std::string::npos) {
    std::string key = line.substr(0, delim_pos);
    std::string value = line.substr(delim_pos + 1);
    base::TrimWhitespaceASCII(key, base::TRIM_ALL, &key);
    base::TrimWhitespaceASCII(value, base::TRIM_ALL, &value);

    // HTTP headers are case insensitive. For simplicity, always use lower-case.
    if (!key.empty() && !value.empty()) {
      headers->insert_or_assign(base::ToLowerASCII(key), value);
    }
  }
  return buf_size.ValueOrDefault(0);
}

size_t LibcurlNetworkFetcherImpl::CurlWriteFileCallback(void* data,
                                                        size_t member_size,
                                                        size_t num_members,
                                                        void* userp) {
  base::CheckedNumeric<size_t> write_size =
      base::CheckedNumeric<size_t>(member_size) *
      base::CheckedNumeric<size_t>(num_members);
  base::File* file = static_cast<base::File*>(userp);

  int bytes_written = file->WriteAtCurrentPos(
      static_cast<const char*>(data), write_size.Cast<int>().ValueOrDefault(0));

  return bytes_written > 0 ? bytes_written : 0;
}

int LibcurlNetworkFetcherImpl::CurlTransferCallback(void* userp,
                                                    curl_off_t dltotal,
                                                    curl_off_t dlnow,
                                                    curl_off_t ultotal,
                                                    curl_off_t ulnow) {
  if (!dltotal && !dlnow && !ultotal && !ulnow) {
    return 0;
  }

  base::WeakPtr<LibcurlNetworkFetcherImpl> wrapper =
      *static_cast<base::WeakPtr<LibcurlNetworkFetcherImpl>*>(userp);
  if (wrapper) {
    if (dltotal || dlnow) {
      wrapper->OnTransferInfo(dltotal, dlnow);
    } else {
      wrapper->OnTransferInfo(ultotal, ulnow);
    }
  }

  return 0;
}

// LibcurlNetworkFetcher wraps a |LibcurlNetworkFetcherImpl| in a
// |base::SequenceBound|. This allows the wrapped fetcher to run solely in a
// dedicated io sequence.
class LibcurlNetworkFetcher : public update_client::NetworkFetcher {
 public:
  using ResponseStartedCallback =
      update_client::NetworkFetcher::ResponseStartedCallback;
  using ProgressCallback = update_client::NetworkFetcher::ProgressCallback;
  using PostRequestCompleteCallback =
      update_client::NetworkFetcher::PostRequestCompleteCallback;
  using DownloadToFileCompleteCallback =
      update_client::NetworkFetcher::DownloadToFileCompleteCallback;

  LibcurlNetworkFetcher() = delete;
  LibcurlNetworkFetcher(const LibcurlNetworkFetcher&) = delete;
  LibcurlNetworkFetcher& operator=(const LibcurlNetworkFetcher&) = delete;
  ~LibcurlNetworkFetcher() override = default;

  explicit LibcurlNetworkFetcher(CurlUniquePtr curl);

  // Overrides for update_client::NetworkFetcher
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
  base::SequenceBound<LibcurlNetworkFetcherImpl> impl_;
};

LibcurlNetworkFetcher::LibcurlNetworkFetcher(CurlUniquePtr curl)
    : impl_(base::ThreadPool::CreateSequencedTaskRunner({base::MayBlock()}),
            std::move(curl),
            base::SequencedTaskRunner::GetCurrentDefault()) {}

void LibcurlNetworkFetcher::PostRequest(
    const GURL& url,
    const std::string& post_data,
    const std::string& content_type,
    const base::flat_map<std::string, std::string>& post_additional_headers,
    ResponseStartedCallback response_started_callback,
    ProgressCallback progress_callback,
    PostRequestCompleteCallback post_request_complete_callback) {
  impl_.AsyncCall(&LibcurlNetworkFetcherImpl::PostRequest)
      .WithArgs(url, post_data, content_type, post_additional_headers,
                std::move(response_started_callback),
                std::move(progress_callback),
                std::move(post_request_complete_callback));
}

base::OnceClosure LibcurlNetworkFetcher::DownloadToFile(
    const GURL& url,
    const base::FilePath& file_path,
    ResponseStartedCallback response_started_callback,
    ProgressCallback progress_callback,
    DownloadToFileCompleteCallback download_to_file_complete_callback) {
  impl_.AsyncCall(&LibcurlNetworkFetcherImpl::DownloadToFile)
      .WithArgs(url, file_path, std::move(response_started_callback),
                std::move(progress_callback),
                std::move(download_to_file_complete_callback));
  return base::DoNothing();
}

}  // namespace

class NetworkFetcherFactory::Impl {};

NetworkFetcherFactory::NetworkFetcherFactory(
    std::optional<PolicyServiceProxyConfiguration>) {}
NetworkFetcherFactory::~NetworkFetcherFactory() = default;

std::unique_ptr<update_client::NetworkFetcher> NetworkFetcherFactory::Create()
    const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CurlUniquePtr curl{curl_easy_init()};
  if (!curl) {
    VLOG(1) << "Failed to initialize a curl handle.";
    return nullptr;
  }
  return std::make_unique<LibcurlNetworkFetcher>(std::move(curl));
}

}  // namespace updater
