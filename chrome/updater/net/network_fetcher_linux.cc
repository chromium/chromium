// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/net/network.h"

#include <curl/curl.h>
#include <dlfcn.h>

#include <array>
#include <memory>
#include <string>
#include <utility>

#include "base/containers/flat_map.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/native_library.h"
#include "base/numerics/checked_math.h"
#include "base/scoped_native_library.h"
#include "base/sequence_checker.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "chrome/updater/policy/service.h"
#include "components/update_client/network.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"

namespace updater {
namespace {

constexpr std::array<const char*, 4> kCurlSOFilenames{
    "libcurl.so",
    "libcurl-gnutls.so.4",
    "libcurl-nss.so.4",
    "libcurl.so.4",
};

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
  ~LibcurlNetworkFetcher() override;

  static std::unique_ptr<LibcurlNetworkFetcher> Create();

  void PostRequest(
      const GURL& url,
      const std::string& post_data,
      const std::string& content_type,
      const base::flat_map<std::string, std::string>& post_additional_headers,
      ResponseStartedCallback response_started_callback,
      ProgressCallback progress_callback,
      PostRequestCompleteCallback post_request_complete_callback) override;

  void DownloadToFile(const GURL& url,
                      const base::FilePath& file_path,
                      ResponseStartedCallback response_started_callback,
                      ProgressCallback progress_callback,
                      DownloadToFileCompleteCallback
                          download_to_file_complete_callback) override;

 private:
  struct LibcurlFunctionPtrs;
  SEQUENCE_CHECKER(sequence_checker_);

  LibcurlNetworkFetcher(CURL* curl,
                        base::ScopedNativeLibrary library,
                        std::unique_ptr<LibcurlFunctionPtrs> curl_functions);

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

  base::raw_ptr<CURL> curl_;
  base::ScopedNativeLibrary library_;
  std::unique_ptr<LibcurlFunctionPtrs> curl_functions_;
  std::array<char, CURL_ERROR_SIZE> curl_error_buf_;

  size_t downloaded_bytes_ = 0;
  ResponseStartedCallback response_started_callback_;
  ProgressCallback progress_callback_;

  base::WeakPtrFactory<LibcurlNetworkFetcher> weak_factory_{this};
};

// Function pointers into the dynamically loaded CURL library.
struct LibcurlNetworkFetcher::LibcurlFunctionPtrs {
  static std::unique_ptr<LibcurlFunctionPtrs> Create(
      base::ScopedNativeLibrary& library) {
    std::unique_ptr<LibcurlFunctionPtrs> curl_functions =
        base::WrapUnique<LibcurlFunctionPtrs>(new LibcurlFunctionPtrs);

    curl_functions->easy_init =
        reinterpret_cast<LibcurlFunctionPtrs::EasyInitFunction>(
            library.GetFunctionPointer("curl_easy_init"));
    curl_functions->easy_setopt =
        reinterpret_cast<LibcurlFunctionPtrs::EasySetOptFunction>(
            library.GetFunctionPointer("curl_easy_setopt"));
    curl_functions->slist_append =
        reinterpret_cast<LibcurlFunctionPtrs::SListAppendFunction>(
            library.GetFunctionPointer("curl_slist_append"));
    curl_functions->slist_free_all =
        reinterpret_cast<LibcurlFunctionPtrs::SListFreeAllFunction>(
            library.GetFunctionPointer("curl_slist_free_all"));
    curl_functions->easy_perform =
        reinterpret_cast<LibcurlFunctionPtrs::EasyPerformFunction>(
            library.GetFunctionPointer("curl_easy_perform"));
    curl_functions->easy_cleanup =
        reinterpret_cast<LibcurlFunctionPtrs::EasyCleanupFunction>(
            library.GetFunctionPointer("curl_easy_cleanup"));
    curl_functions->easy_getinfo =
        reinterpret_cast<LibcurlFunctionPtrs::EasyGetInfoFunction>(
            library.GetFunctionPointer("curl_easy_getinfo"));
    curl_functions->easy_reset =
        reinterpret_cast<LibcurlFunctionPtrs::EasyResetFunction>(
            library.GetFunctionPointer("curl_easy_reset"));

    if (curl_functions->easy_init && curl_functions->easy_setopt &&
        curl_functions->slist_append && curl_functions->slist_free_all &&
        curl_functions->easy_perform && curl_functions->easy_cleanup &&
        curl_functions->easy_getinfo && curl_functions->easy_reset)
      return curl_functions;
    return nullptr;
  }

  using EasyInitFunction = CURL* (*)();
  [[nodiscard]] EasyInitFunction easy_init = nullptr;

  using EasySetOptFunction = CURLcode (*)(CURL*, CURLoption, ...);
  [[nodiscard]] EasySetOptFunction easy_setopt = nullptr;

  using SListAppendFunction = struct curl_slist* (*)(struct curl_slist*,
                                                     const char*);
  [[nodiscard]] SListAppendFunction slist_append = nullptr;

  using SListFreeAllFunction = void (*)(struct curl_slist*);
  SListFreeAllFunction slist_free_all = nullptr;

  using EasyPerformFunction = CURLcode (*)(CURL*);
  [[nodiscard]] EasyPerformFunction easy_perform = nullptr;

  using EasyCleanupFunction = void (*)(CURL*);
  EasyCleanupFunction easy_cleanup = nullptr;

  using EasyGetInfoFunction = CURLcode (*)(CURL*, CURLINFO info, ...);
  [[nodiscard]] EasyGetInfoFunction easy_getinfo = nullptr;

  using EasyResetFunction = void (*)(CURL*);
  EasyResetFunction easy_reset = nullptr;

 private:
  LibcurlFunctionPtrs() = default;
};

LibcurlNetworkFetcher::LibcurlNetworkFetcher(
    CURL* curl,
    base::ScopedNativeLibrary library,
    std::unique_ptr<LibcurlFunctionPtrs> curl_functions)
    : curl_(curl),
      library_(std::move(library)),
      curl_functions_(std::move(curl_functions)) {
  CHECK(curl_functions_);
}

LibcurlNetworkFetcher::~LibcurlNetworkFetcher() {
  curl_functions_->easy_cleanup(curl_);
}

std::unique_ptr<LibcurlNetworkFetcher> LibcurlNetworkFetcher::Create() {
  base::NativeLibrary native_library;
  for (const char* name : kCurlSOFilenames) {
    native_library = base::LoadNativeLibrary(base::FilePath(name), nullptr);
    if (native_library)
      break;
  }
  if (!native_library) {
    VLOG(1) << "Could not dynamically load libcurl.";
    return nullptr;
  }
  base::ScopedNativeLibrary scoped_library(std::move(native_library));

  std::unique_ptr<LibcurlFunctionPtrs> curl_functions =
      LibcurlFunctionPtrs::Create(scoped_library);
  if (!curl_functions) {
    VLOG(1) << "Failed to get libcurl function pointers.";
    return nullptr;
  }

  raw_ptr<CURL> curl = curl_functions->easy_init();
  if (!curl) {
    VLOG(1) << "Failed to initialize a curl handle.";
    return nullptr;
  }

  return base::WrapUnique<LibcurlNetworkFetcher>(new LibcurlNetworkFetcher(
      curl, std::move(scoped_library), std::move(curl_functions)));
}

void LibcurlNetworkFetcher::PostRequest(
    const GURL& url,
    const std::string& post_data,
    const std::string& content_type,
    const base::flat_map<std::string, std::string>& post_additional_headers,
    ResponseStartedCallback response_started_callback,
    ProgressCallback progress_callback,
    PostRequestCompleteCallback post_request_complete_callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  VLOG(2) << __func__;

  curl_functions_->easy_reset(curl_);

  base::raw_ptr<struct curl_slist> headers = nullptr;
  headers = curl_functions_->slist_append(
      headers, ("Content-Type: " + content_type).c_str());
  for (const auto& [key, value] : post_additional_headers) {
    headers =
        curl_functions_->slist_append(headers, (key + ": " + value).c_str());
  }

  base::flat_map<std::string, std::string> response_headers;
  std::unique_ptr<std::string> response_body = std::make_unique<std::string>();

  base::WeakPtr<LibcurlNetworkFetcher> weak_ptr = weak_factory_.GetWeakPtr();
  if (curl_functions_->easy_setopt(curl_, CURLOPT_URL, url.spec().c_str()) ||
      curl_functions_->easy_setopt(curl_, CURLOPT_HTTPPOST, 1L) ||
      curl_functions_->easy_setopt(curl_, CURLOPT_HTTPHEADER, headers.get()) ||
      curl_functions_->easy_setopt(curl_, CURLOPT_POSTFIELDSIZE,
                                   post_data.size()) ||
      curl_functions_->easy_setopt(curl_, CURLOPT_POSTFIELDS,
                                   post_data.c_str()) ||
      curl_functions_->easy_setopt(
          curl_, CURLOPT_HEADERFUNCTION,
          &LibcurlNetworkFetcher::CurlHeaderCallback) ||
      curl_functions_->easy_setopt(curl_, CURLOPT_HEADERDATA,
                                   &response_headers) ||
      curl_functions_->easy_setopt(
          curl_, CURLOPT_WRITEFUNCTION,
          &LibcurlNetworkFetcher::CurlWriteStringCallback) ||
      curl_functions_->easy_setopt(curl_, CURLOPT_WRITEDATA,
                                   response_body.get()) ||
      curl_functions_->easy_setopt(curl_, CURLOPT_NOPROGRESS, 0) ||
      curl_functions_->easy_setopt(
          curl_, CURLOPT_XFERINFOFUNCTION,
          &LibcurlNetworkFetcher::CurlTransferCallback) ||
      curl_functions_->easy_setopt(curl_, CURLOPT_XFERINFODATA, &weak_ptr) ||
      curl_functions_->easy_setopt(curl_, CURLOPT_ERRORBUFFER,
                                   curl_error_buf_.data())) {
    VLOG(1) << "Failed to set curl options for HTTP POST.";
    curl_functions_->slist_free_all(headers);
    return;
  }

  response_started_callback_ = std::move(response_started_callback);
  progress_callback_ = std::move(progress_callback);

  CURLcode result = curl_functions_->easy_perform(curl_);
  if (result != CURLE_OK) {
    VLOG(1) << "Failed to perform HTTP POST. "
            << (curl_error_buf_[0] ? curl_error_buf_.data() : "")
            << " (CURLcode " << result << ")";
  }

  int x_retry_after = -1;
  if (response_headers.contains(
          update_client::NetworkFetcher::kHeaderXRetryAfter)) {
    if (!base::StringToInt(
            response_headers.at(
                update_client::NetworkFetcher::kHeaderXRetryAfter),
            &x_retry_after)) {
      x_retry_after = -1;
    }
  } else {
    x_retry_after = -1;
  }

  std::move(post_request_complete_callback)
      .Run(std::move(response_body), CURLE_OK,
           response_headers.contains(update_client::NetworkFetcher::kHeaderEtag)
               ? response_headers.at(update_client::NetworkFetcher::kHeaderEtag)
               : "",
           response_headers.contains(
               update_client::NetworkFetcher::kHeaderXCupServerProof)
               ? response_headers.at(
                     update_client::NetworkFetcher::kHeaderXCupServerProof)
               : "",
           x_retry_after);

  curl_functions_->slist_free_all(headers);
}

void LibcurlNetworkFetcher::DownloadToFile(
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
    VLOG(1) << "LibcurlNetworkFetcher cannot open file for download.";
    std::move(download_to_file_complete_callback).Run(CURLE_WRITE_ERROR, 0);
    return;
  }

  curl_functions_->easy_reset(curl_);

  base::WeakPtr<LibcurlNetworkFetcher> weak_ptr = weak_factory_.GetWeakPtr();
  if (curl_functions_->easy_setopt(curl_, CURLOPT_URL, url.spec().c_str()) ||
      curl_functions_->easy_setopt(curl_, CURLOPT_HTTPGET, 1L) ||
      curl_functions_->easy_setopt(
          curl_, CURLOPT_WRITEFUNCTION,
          &LibcurlNetworkFetcher::CurlWriteFileCallback) ||
      curl_functions_->easy_setopt(curl_, CURLOPT_WRITEDATA, &file) ||
      curl_functions_->easy_setopt(curl_, CURLOPT_NOPROGRESS, 0) ||
      curl_functions_->easy_setopt(
          curl_, CURLOPT_XFERINFOFUNCTION,
          &LibcurlNetworkFetcher::CurlTransferCallback) ||
      curl_functions_->easy_setopt(curl_, CURLOPT_XFERINFODATA, &weak_ptr) ||
      curl_functions_->easy_setopt(curl_, CURLOPT_ERRORBUFFER,
                                   curl_error_buf_.data())) {
    VLOG(1) << "Failed to set curl options for HTTP GET.";
    return;
  }

  response_started_callback_ = std::move(response_started_callback);
  progress_callback_ = std::move(progress_callback);

  downloaded_bytes_ = 0;
  curl_error_buf_[0] = '\0';
  CURLcode result = curl_functions_->easy_perform(curl_);
  if (result != CURLE_OK) {
    VLOG(1) << "Failed to perform HTTP GET. "
            << (curl_error_buf_[0] ? curl_error_buf_.data() : "")
            << " (CURLcode " << result << ")";
  }

  file.Close();
  std::move(download_to_file_complete_callback).Run(result, downloaded_bytes_);
}

void LibcurlNetworkFetcher::OnTransferInfo(curl_off_t total,
                                           curl_off_t current) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (response_started_callback_ && total) {
    // Query for an HTTP response code. If one has not been sent yet, the
    // transfer has not started.
    long response_code = 0;
    if (curl_functions_->easy_getinfo(curl_, CURLINFO_RESPONSE_CODE,
                                      &response_code) != CURLE_OK) {
      VLOG(1) << "Cannot retrieve HTTP response code for ongoing transfer.";
      return;
    } else if (response_code) {
      std::move(response_started_callback_).Run(response_code, total);
    }
  }

  if (progress_callback_ && current)
    progress_callback_.Run(current);
}

size_t LibcurlNetworkFetcher::CurlWriteStringCallback(void* data,
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

size_t LibcurlNetworkFetcher::CurlHeaderCallback(char* data,
                                                 size_t member_size,
                                                 size_t num_members,
                                                 void* userp) {
  base::raw_ptr<base::flat_map<std::string, std::string>> headers =
      static_cast<base::flat_map<std::string, std::string>*>(userp);

  base::CheckedNumeric<size_t> buf_size =
      base::CheckedNumeric<size_t>(member_size) *
      base::CheckedNumeric<size_t>(num_members);

  std::string line(data, buf_size.ValueOrDefault(0));
  // Reject any headers that aren't compliant with RFC 5987.
  // Returning 0 will abort the transfer.
  if (!base::IsStringASCII(line))
    return 0;

  size_t delim_pos = line.find(":");
  if (delim_pos != std::string::npos) {
    std::string key = line.substr(0, delim_pos);
    std::string value = line.substr(delim_pos + 1);
    base::TrimWhitespaceASCII(key, base::TRIM_ALL, &key);
    base::TrimWhitespaceASCII(value, base::TRIM_ALL, &value);

    if (!key.empty() && !value.empty())
      headers->insert_or_assign(key, value);
  }
  return buf_size.ValueOrDefault(0);
}

size_t LibcurlNetworkFetcher::CurlWriteFileCallback(void* data,
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

int LibcurlNetworkFetcher::CurlTransferCallback(void* userp,
                                                curl_off_t dltotal,
                                                curl_off_t dlnow,
                                                curl_off_t ultotal,
                                                curl_off_t ulnow) {
  if (!dltotal && !dlnow && !ultotal && !ulnow)
    return 0;

  base::WeakPtr<LibcurlNetworkFetcher> wrapper =
      *static_cast<base::WeakPtr<LibcurlNetworkFetcher>*>(userp);
  if (wrapper) {
    if (dltotal || dlnow)
      wrapper->OnTransferInfo(dltotal, dlnow);
    else
      wrapper->OnTransferInfo(ultotal, ulnow);
  }

  return 0;
}

}  // namespace

class NetworkFetcherFactory::Impl {};

NetworkFetcherFactory::NetworkFetcherFactory(
    absl::optional<PolicyServiceProxyConfiguration>) {}
NetworkFetcherFactory::~NetworkFetcherFactory() = default;

std::unique_ptr<update_client::NetworkFetcher> NetworkFetcherFactory::Create()
    const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return LibcurlNetworkFetcher::Create();
}

}  // namespace updater
