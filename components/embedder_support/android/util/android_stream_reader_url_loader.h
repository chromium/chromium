// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_EMBEDDER_SUPPORT_ANDROID_UTIL_ANDROID_STREAM_READER_URL_LOADER_H_
#define COMPONENTS_EMBEDDER_SUPPORT_ANDROID_UTIL_ANDROID_STREAM_READER_URL_LOADER_H_

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/threading/thread_checker.h"
#include "base/time/time.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/system/simple_watcher.h"
#include "net/http/http_byte_range.h"
#include "services/network/public/cpp/net_adapters.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/mojom/url_loader.mojom.h"
#include "services/network/public/mojom/url_response_head.mojom.h"

namespace embedder_support {
class InputStream;
class InputStreamReaderWrapper;

// Custom URLLoader implementation for loading responses from Android
// InputStreams. Although this works generally for implementers of the
// ResponseDelegate interface, this specifically aims to support:
//
//  - shouldInterceptRequest callback
//  - content:// URLs, which load content from Android ContentProviders (which
//    could be in-app or come from other apps)
//  - file:///android_asset/ & file:///android_res/ URLs, which load in-app
//    content from the app's asset/ and res/ folders
class AndroidStreamReaderURLLoader : public network::mojom::URLLoader {
 public:
  // Delegate abstraction for obtaining input streams.
  class ResponseDelegate {
   public:
    virtual ~ResponseDelegate() = default;

    // This method is called from a worker thread, not from the IO thread.
    virtual std::unique_ptr<embedder_support::InputStream> OpenInputStream(
        JNIEnv* env) = 0;

    // All the methods below are called on the URLLoader thread (IO thread).

    // This method is called if the result of calling OpenInputStream was null.
    // Returns true if the request was restarted with a new loader or
    // was completed, false otherwise.
    virtual bool OnInputStreamOpenFailed() = 0;

    // Allows the delegate to update the mime type, by setting |mime_type| and
    // returning true.
    virtual bool GetMimeType(JNIEnv* env,
                             const GURL& url,
                             embedder_support::InputStream* stream,
                             std::string* mime_type) = 0;

    // Allows the delegate to set the charset of the response by setting
    // |charset|.
    virtual void GetCharset(JNIEnv* env,
                            const GURL& url,
                            embedder_support::InputStream* stream,
                            std::string* charset) = 0;

    // Allows the delegate to add extra response headers.
    virtual void AppendResponseHeaders(JNIEnv* env,
                                       net::HttpResponseHeaders* headers) = 0;

    // Called right before URLLoaderClient::OnReceiveResponse is called. If this
    // method returns true then a copy of the response is made and returned in
    // OnResponseCache.
    virtual bool ShouldCacheResponse(network::mojom::URLResponseHead* response);

    // Called if ShouldCacheResponse returned true and the response was
    // successful.
    virtual void OnResponseCache(const std::string& data) {}
  };

  struct SecurityOptions {
    bool disable_web_security = false;
    bool allow_cors_to_same_scheme = false;
  };

  using SetCookieHeader = base::RepeatingCallback<void(
      const network::ResourceRequest& request,
      const std::string& value,
      const std::optional<base::Time>& server_time)>;

  AndroidStreamReaderURLLoader(
      const network::ResourceRequest& resource_request,
      mojo::PendingRemote<network::mojom::URLLoaderClient> client,
      const net::MutableNetworkTrafficAnnotationTag& traffic_annotation,
      std::unique_ptr<ResponseDelegate> response_delegate,
      std::optional<SecurityOptions> security_options,
      std::optional<SetCookieHeader> set_cookie_header = std::nullopt);

  AndroidStreamReaderURLLoader(const AndroidStreamReaderURLLoader&) = delete;
  AndroidStreamReaderURLLoader& operator=(const AndroidStreamReaderURLLoader&) =
      delete;

  ~AndroidStreamReaderURLLoader() override;

  void Start(std::unique_ptr<InputStream> input_stream);

  // network::mojom::URLLoader overrides:
  void FollowRedirect(
      const std::vector<std::string>& removed_headers,
      const net::HttpRequestHeaders& modified_headers,
      const net::HttpRequestHeaders& modified_cors_exempt_headers,
      const std::optional<GURL>& new_url) override;
  void SetPriority(net::RequestPriority priority,
                   int intra_priority_value) override;
  void PauseReadingBodyFromNet() override;
  void ResumeReadingBodyFromNet() override;

 private:
  bool ParseRange(const net::HttpRequestHeaders& headers);
  void OnInputStreamOpened(
      std::unique_ptr<AndroidStreamReaderURLLoader::ResponseDelegate>
          returned_delegate,
      std::unique_ptr<embedder_support::InputStream> input_stream);
  void OnReaderSeekCompleted(int result);
  void HeadersComplete(int status_code, const std::string& status_text);
  void RequestCompleteWithStatus(
      const network::URLLoaderCompletionStatus& status);
  void RequestComplete(int status_code);
  void SendBody();

  void OnDataPipeWritable(MojoResult result);
  void CleanUp();

  // Called after trying to read some bytes from the stream. |result| can be a
  // positive number (the number of bytes read), zero (no bytes were read
  // because the stream is finished), or negative (error condition).
  void DidRead(int result);
  // Reads some bytes from the stream. Calls |DidRead| after each read (also, in
  // the case where it fails to read due to an error).
  void ReadMore();
  // The Set-Cookie header can't be sent over IPC. Because this stream reader
  // bypasses the network stack, the Set-Cookie is ignored entirely.
  // This is called before sending the response to give the embedder
  // an opportunity to save headers.
  void SetCookies();
  // Send response headers and the data pipe consumer handle (for the body) to
  // the URLLoaderClient. Requires |consumer_handle_| to be valid, and will make
  // |consumer_handle_| invalid after running.
  void SendResponseToClient();

  // Expected content size
  int64_t expected_content_size_ = -1;
  mojo::ScopedDataPipeConsumerHandle consumer_handle_;

  net::HttpByteRange byte_range_;
  network::ResourceRequest resource_request_;
  network::mojom::URLResponseHeadPtr response_head_;
  bool reject_cors_request_;
  mojo::Remote<network::mojom::URLLoaderClient> client_;
  const net::MutableNetworkTrafficAnnotationTag traffic_annotation_;
  std::unique_ptr<ResponseDelegate> response_delegate_;
  scoped_refptr<InputStreamReaderWrapper> input_stream_reader_wrapper_;

  // If true, a copy of the response is made.
  bool cache_response_ = false;
  std::string cached_response_;

  mojo::ScopedDataPipeProducerHandle producer_handle_;
  scoped_refptr<network::NetToMojoPendingBuffer> pending_buffer_;
  mojo::SimpleWatcher writable_handle_watcher_;
  base::Time start_time_;
  std::optional<SetCookieHeader> set_cookie_header_;
  base::ThreadChecker thread_checker_;

  base::WeakPtrFactory<AndroidStreamReaderURLLoader> weak_factory_{this};
};

}  // namespace embedder_support

#endif  // COMPONENTS_EMBEDDER_SUPPORT_ANDROID_UTIL_ANDROID_STREAM_READER_URL_LOADER_H_
