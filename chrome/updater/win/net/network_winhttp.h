// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_WIN_NET_NETWORK_WINHTTP_H_
#define CHROME_UPDATER_WIN_NET_NETWORK_WINHTTP_H_

#include <windows.h>

#include <stdint.h>

#include <memory>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/strings/string_piece_forward.h"
#include "base/threading/thread_checker.h"
#include "chrome/updater/win/net/scoped_hinternet.h"
#include "components/update_client/network.h"
#include "url/gurl.h"

namespace base {
class SingleThreadTaskRunner;
}

namespace updater {

// Implements a network fetcher in terms of WinHTTP. The class is ref-counted
// as it is accessed from both the main thread and the worker threads in
// WinHTTP.
class NetworkFetcherWinHTTP
    : public base::RefCountedThreadSafe<NetworkFetcherWinHTTP> {
 public:
  using FetchCompleteCallback = base::OnceCallback<void()>;
  using FetchStartedCallback =
      update_client::NetworkFetcher::ResponseStartedCallback;
  using FetchProgressCallback = update_client::NetworkFetcher::ProgressCallback;

  explicit NetworkFetcherWinHTTP(const HINTERNET& session_handle_);

  void Close();

  void PostRequest(
      const GURL& url,
      const std::string& post_data,
      const base::flat_map<std::string, std::string>& post_additional_headers,
      FetchStartedCallback fetch_started_callback,
      FetchProgressCallback fetch_progress_callback,
      FetchCompleteCallback fetch_complete_callback);
  void DownloadToFile(const GURL& url,
                      const base::FilePath& file_path,
                      FetchStartedCallback fetch_started_callback,
                      FetchProgressCallback fetch_progress_callback,
                      FetchCompleteCallback fetch_complete_callback);

  std::string GetResponseBody() const;
  HRESULT GetNetError() const;
  std::string GetHeaderETag() const;
  int64_t GetXHeaderRetryAfterSec() const;
  base::FilePath GetFilePath() const;
  int64_t GetContentSize() const;

 private:
  friend class base::RefCountedThreadSafe<NetworkFetcherWinHTTP>;
  using WriteDataCallback = base::RepeatingCallback<void()>;

  ~NetworkFetcherWinHTTP();

  static void __stdcall WinHttpStatusCallback(HINTERNET handle,
                                              DWORD_PTR context,
                                              DWORD status,
                                              void* info,
                                              DWORD info_len);

  DWORD_PTR context() const { return reinterpret_cast<DWORD_PTR>(this); }

  void StatusCallback(HINTERNET handle,
                      uint32_t status,
                      void* info,
                      uint32_t info_len);

  HRESULT BeginFetch(
      const std::string& data,
      const base::flat_map<std::string, std::string>& additional_headers);
  scoped_hinternet Connect();
  scoped_hinternet OpenRequest();
  HRESULT SendRequest(const std::string& data);
  void SendRequestComplete();
  HRESULT ReceiveResponse();
  void ReceiveResponseComplete();
  HRESULT QueryDataAvailable();
  void QueryDataAvailableComplete(size_t num_bytes_available);
  HRESULT ReadData(size_t num_bytes_available);
  void ReadDataComplete(size_t num_bytes_read);
  void RequestError(const WINHTTP_ASYNC_RESULT* result);

  void WriteDataToMemory();
  void WriteDataToFile();
  bool WriteDataToFileBlocking();
  void WriteDataToFileComplete(bool is_eof);

  THREAD_CHECKER(thread_checker_);
  scoped_refptr<base::SingleThreadTaskRunner> main_thread_task_runner_;

  const HINTERNET& session_handle_;  // Owned by NetworkFetcherWinHTTPFactory.
  scoped_hinternet connect_handle_;
  scoped_hinternet request_handle_;

  // Keeps an outstanding reference count on itself as long as there is a
  // valid request handle and the context for the handle is set to this
  // instance.
  scoped_refptr<NetworkFetcherWinHTTP> self_;

  GURL url_;
  bool is_https_ = false;
  std::string host_;
  int port_ = 0;
  std::string path_for_request_;

  base::StringPiece16 verb_;
  base::StringPiece16 content_type_;
  WriteDataCallback write_data_callback_;
  HRESULT net_error_ = S_OK;
  std::string etag_;
  int64_t xheader_retry_after_sec_ = -1;
  std::vector<char> read_buffer_;
  std::string post_response_body_;
  base::FilePath file_path_;
  base::File file_;
  int64_t content_size_ = 0;

  FetchStartedCallback fetch_started_callback_;
  FetchProgressCallback fetch_progress_callback_;
  FetchCompleteCallback fetch_complete_callback_;

  scoped_refptr<update_client::NetworkFetcherFactory> network_fetcher_factory_;

  DISALLOW_COPY_AND_ASSIGN(NetworkFetcherWinHTTP);
};

}  // namespace updater

#endif  // CHROME_UPDATER_WIN_NET_NETWORK_WINHTTP_H_
