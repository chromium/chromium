// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/net/network.h"

#include <curl/curl.h>
#include <curl/system.h>
#include <dlfcn.h>

#include <array>
#include <memory>
#include <string>
#include <utility>

#include "base/containers/flat_map.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/native_library.h"
#include "base/numerics/checked_math.h"
#include "base/scoped_native_library.h"
#include "base/sequence_checker.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
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

// Helper class to allow for creating refcounted ScopedNativeLibrarys that can
// outlive the NetworkFetcherFactory.
class RefCountableNativeLibrary
    : public base::ScopedNativeLibrary,
      public base::RefCountedThreadSafe<RefCountableNativeLibrary> {
 public:
  // Takes ownership of the given library handle.
  explicit RefCountableNativeLibrary(base::NativeLibrary library)
      : base::ScopedNativeLibrary(std::move(library)) {}

 private:
  friend class base::RefCountedThreadSafe<RefCountableNativeLibrary>;
  ~RefCountableNativeLibrary() override = default;
};

// Function pointers into the dynamically loaded CURL library.
struct LibcurlFunctionPtrs
    : public base::RefCountedThreadSafe<LibcurlFunctionPtrs> {
  static scoped_refptr<LibcurlFunctionPtrs> Create(
      scoped_refptr<RefCountableNativeLibrary> library) {
    if (!library)
      return nullptr;

    scoped_refptr<LibcurlFunctionPtrs> curl_functions =
        base::WrapRefCounted(new LibcurlFunctionPtrs);
    curl_functions->easy_init =
        reinterpret_cast<LibcurlFunctionPtrs::EasyInitFunction>(
            library->GetFunctionPointer("curl_easy_init"));
    curl_functions->easy_setopt =
        reinterpret_cast<LibcurlFunctionPtrs::EasySetOptFunction>(
            library->GetFunctionPointer("curl_easy_setopt"));
    curl_functions->slist_append =
        reinterpret_cast<LibcurlFunctionPtrs::SListAppendFunction>(
            library->GetFunctionPointer("curl_slist_append"));
    curl_functions->slist_free_all =
        reinterpret_cast<LibcurlFunctionPtrs::SListFreeAllFunction>(
            library->GetFunctionPointer("curl_slist_free_all"));
    curl_functions->easy_perform =
        reinterpret_cast<LibcurlFunctionPtrs::EasyPerformFunction>(
            library->GetFunctionPointer("curl_easy_perform"));
    curl_functions->easy_cleanup =
        reinterpret_cast<LibcurlFunctionPtrs::EasyCleanupFunction>(
            library->GetFunctionPointer("curl_easy_cleanup"));
    curl_functions->easy_getinfo =
        reinterpret_cast<LibcurlFunctionPtrs::EasyGetInfoFunction>(
            library->GetFunctionPointer("curl_easy_getinfo"));
    curl_functions->easy_reset =
        reinterpret_cast<LibcurlFunctionPtrs::EasyResetFunction>(
            library->GetFunctionPointer("curl_easy_reset"));

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
  friend class base::RefCountedThreadSafe<LibcurlFunctionPtrs>;
  LibcurlFunctionPtrs() = default;
  ~LibcurlFunctionPtrs() = default;
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

  LibcurlNetworkFetcher(CURL* curl,
                        scoped_refptr<RefCountableNativeLibrary> library,
                        scoped_refptr<LibcurlFunctionPtrs> curl_functions);

  // Overrides for update_client::NetworkFetcher
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
  SEQUENCE_CHECKER(sequence_checker_);

  void PostRequestOnIOSequence(
      const GURL& url,
      const std::string& post_data,
      const std::string& content_type,
      const base::flat_map<std::string, std::string>& post_additional_headers,
      ResponseStartedCallback response_started_callback,
      ProgressCallback progress_callback,
      PostRequestCompleteCallback post_request_complete_callback);

  void DownloadToFileOnIOSequence(
      const GURL& url,
      const base::FilePath& file_path,
      ResponseStartedCallback response_started_callback,
      ProgressCallback progress_callback,
      DownloadToFileCompleteCallback download_to_file_complete_callback);

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

  // Sequence to perform blocking IO.
  scoped_refptr<base::SequencedTaskRunner> io_sequence_ =
      base::ThreadPool::CreateSequencedTaskRunner({base::MayBlock()});
  // Sequence to post callbacks to.
  scoped_refptr<base::SequencedTaskRunner> callback_sequence_ =
      base::SequencedTaskRunner::GetCurrentDefault();

  base::raw_ptr<CURL> curl_;
  scoped_refptr<RefCountableNativeLibrary> library_;
  scoped_refptr<LibcurlFunctionPtrs> curl_functions_;
  std::array<char, CURL_ERROR_SIZE> curl_error_buf_;

  ResponseStartedCallback response_started_callback_;
  ProgressCallback progress_callback_;

  base::WeakPtrFactory<LibcurlNetworkFetcher> weak_factory_{this};
};

LibcurlNetworkFetcher::LibcurlNetworkFetcher(
    CURL* curl,
    scoped_refptr<RefCountableNativeLibrary> library,
    scoped_refptr<LibcurlFunctionPtrs> curl_functions)
    : curl_(curl),
      library_(library),
      curl_functions_(std::move(curl_functions)) {
  CHECK(curl_functions_);
}

LibcurlNetworkFetcher::~LibcurlNetworkFetcher() {
  curl_functions_->easy_cleanup(curl_);
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

  io_sequence_->PostTask(
      FROM_HERE, base::BindOnce(&LibcurlNetworkFetcher::PostRequestOnIOSequence,
                                weak_factory_.GetWeakPtr(), url, post_data,
                                content_type, post_additional_headers,
                                std::move(response_started_callback),
                                std::move(progress_callback),
                                std::move(post_request_complete_callback)));
}

void LibcurlNetworkFetcher::PostRequestOnIOSequence(
    const GURL& url,
    const std::string& post_data,
    const std::string& content_type,
    const base::flat_map<std::string, std::string>& post_additional_headers,
    ResponseStartedCallback response_started_callback,
    ProgressCallback progress_callback,
    PostRequestCompleteCallback post_request_complete_callback) {
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

  io_sequence_->PostTask(
      FROM_HERE,
      base::BindOnce(&LibcurlNetworkFetcher::DownloadToFileOnIOSequence,
                     weak_factory_.GetWeakPtr(), url, file_path,
                     std::move(response_started_callback),
                     std::move(progress_callback),
                     std::move(download_to_file_complete_callback)));
}

void LibcurlNetworkFetcher::DownloadToFileOnIOSequence(
    const GURL& url,
    const base::FilePath& file_path,
    ResponseStartedCallback response_started_callback,
    ProgressCallback progress_callback,
    DownloadToFileCompleteCallback download_to_file_complete_callback) {
  base::File file;
  file.Initialize(file_path, base::File::Flags::FLAG_CREATE_ALWAYS |
                                 base::File::Flags::FLAG_WRITE);
  if (!file.IsValid()) {
    VLOG(1) << "LibcurlNetworkFetcher cannot open file for download.";
    callback_sequence_->PostTask(
        FROM_HERE, base::BindOnce(std::move(download_to_file_complete_callback),
                                  CURLE_WRITE_ERROR, 0));
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

  curl_off_t downloaded_bytes = 0;
  curl_error_buf_[0] = '\0';
  CURLcode result = curl_functions_->easy_perform(curl_);
  if (result != CURLE_OK) {
    VLOG(1) << "Failed to perform HTTP GET. "
            << (curl_error_buf_[0] ? curl_error_buf_.data() : "")
            << " (CURLcode " << result << ")";
  } else if (curl_functions_->easy_getinfo(curl_, CURLINFO_SIZE_DOWNLOAD_T,
                                           &downloaded_bytes) != CURLE_OK) {
    VLOG(1) << "Cannot retrieve downloaded bytes for finished trasnfer";
    downloaded_bytes = 0;
  }

  file.Close();
  callback_sequence_->PostTask(
      FROM_HERE, base::BindOnce(std::move(download_to_file_complete_callback),
                                result, downloaded_bytes));
}

void LibcurlNetworkFetcher::OnTransferInfo(curl_off_t total,
                                           curl_off_t current) {
  if (response_started_callback_ && total) {
    // Query for an HTTP response code. If one has not been sent yet, the
    // transfer has not started.
    long response_code = 0;
    if (curl_functions_->easy_getinfo(curl_, CURLINFO_RESPONSE_CODE,
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

    // HTTP headers are case insensitive. For simplicity, always use lower-case.
    if (!key.empty() && !value.empty())
      headers->insert_or_assign(base::ToLowerASCII(key), value);
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

// Perform blocking IO to load libcurl when the NetworkFetcherFactory is
// created. As long as the NetworkFetcherFactory is alive the library will not
// need to be reloaded when creating LibcurlNetworkFetcher instances.
class NetworkFetcherFactory::Impl {
 public:
  Impl();
  std::unique_ptr<update_client::NetworkFetcher> Create();

 private:
  scoped_refptr<RefCountableNativeLibrary> library_;
  scoped_refptr<LibcurlFunctionPtrs> functions_;
};

NetworkFetcherFactory::Impl::Impl() {
  base::NativeLibrary native_library;
  for (const char* name : kCurlSOFilenames) {
    native_library = base::LoadNativeLibrary(base::FilePath(name), nullptr);
    if (native_library)
      break;
  }
  if (!native_library) {
    VLOG(1) << "Could not dynamically load libcurl.";
    return;
  }
  scoped_refptr<RefCountableNativeLibrary> scoped_library =
      base::MakeRefCounted<RefCountableNativeLibrary>(
          std::move(native_library));

  scoped_refptr<LibcurlFunctionPtrs> curl_functions =
      LibcurlFunctionPtrs::Create(scoped_library);
  if (!curl_functions) {
    VLOG(1) << "Failed to get libcurl function pointers.";
    return;
  }

  library_ = scoped_library;
  functions_ = curl_functions;
}

std::unique_ptr<update_client::NetworkFetcher>
NetworkFetcherFactory::Impl::Create() {
  if (!library_ || !functions_)
    return nullptr;

  raw_ptr<CURL> curl = functions_->easy_init();
  if (!curl) {
    VLOG(1) << "Failed to initialize a curl handle.";
    return nullptr;
  }

  return std::make_unique<LibcurlNetworkFetcher>(curl, library_, functions_);
}

NetworkFetcherFactory::NetworkFetcherFactory(
    absl::optional<PolicyServiceProxyConfiguration>)
    : impl_(std::make_unique<Impl>()) {}
NetworkFetcherFactory::~NetworkFetcherFactory() = default;

std::unique_ptr<update_client::NetworkFetcher> NetworkFetcherFactory::Create()
    const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return impl_->Create();
}

}  // namespace updater
