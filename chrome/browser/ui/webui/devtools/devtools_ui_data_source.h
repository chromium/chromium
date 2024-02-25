// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_DEVTOOLS_DEVTOOLS_UI_DATA_SOURCE_H_
#define CHROME_BROWSER_UI_WEBUI_DEVTOOLS_DEVTOOLS_UI_DATA_SOURCE_H_

#include <list>
#include <memory>

#include "content/public/browser/url_data_source.h"

#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "third_party/blink/public/public_buildflags.h"

class GURL;

namespace net {
struct NetworkTrafficAnnotationTag;
}

// An URLDataSource implementation that handles devtools://devtools/
// requests. Three types of requests could be handled based on the URL path:
// 1. /bundled/: bundled DevTools frontend is served. The path can be provided
//    via --custom-devtools-frontend as file:// URL.
// 2. /remote/: remote DevTools frontend is served from App Engine.
// 3. /custom/: custom DevTools frontend is served from the server as specified
//    via --custom-devtools-frontend as http:// URL.
class DevToolsDataSource : public content::URLDataSource {
 public:
  using GotDataCallback = content::URLDataSource::GotDataCallback;

  explicit DevToolsDataSource(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory);

  DevToolsDataSource(const DevToolsDataSource&) = delete;
  DevToolsDataSource& operator=(const DevToolsDataSource&) = delete;

  ~DevToolsDataSource() override;

  // content::URLDataSource implementation.
  std::string GetSource() override;

  void StartDataRequest(const GURL& url,
                        const content::WebContents::Getter& wc_getter,
                        GotDataCallback callback) override;

 private:
  friend class DevToolsUIDataSourceTest;

  struct PendingRequest;

  // content::URLDataSource overrides.
  std::string GetMimeType(const GURL& url) override;
  bool ShouldAddContentSecurityPolicy() override;
  bool ShouldDenyXFrameOptions() override;
  bool ShouldServeMimeTypeAsContentTypeHeader() override;

  void OnLoadComplete(std::list<PendingRequest>::iterator request_iter,
                      std::unique_ptr<std::string> response_body);

  // Serves bundled DevTools frontend from ResourceBundle.
  void StartBundledDataRequest(const std::string& path,
                               GotDataCallback callback);

  // Serves remote DevTools frontend from hard-coded App Engine domain.
  void StartRemoteDataRequest(const GURL& url, GotDataCallback callback);

  // Serves remote DevTools frontend from any endpoint, passed through
  // command-line flag.
  void StartCustomDataRequest(const GURL& url, GotDataCallback callback);

  bool MaybeHandleCustomRequest(const std::string& path,
                                GotDataCallback* callback);

  virtual void StartNetworkRequest(
      const GURL& url,
      const net::NetworkTrafficAnnotationTag& traffic_annotation,
      int load_flags,
      GotDataCallback callback);

  virtual void StartFileRequest(const std::string& path,
                                GotDataCallback callback);

  struct PendingRequest {
    PendingRequest();
    PendingRequest(const PendingRequest&) = delete;
    PendingRequest& operator=(const PendingRequest&) = delete;
    PendingRequest(PendingRequest&& other);
    PendingRequest& operator=(PendingRequest&& other) = default;
    ~PendingRequest();

    GotDataCallback callback;
    std::unique_ptr<network::SimpleURLLoader> loader;
  };

  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
  std::list<PendingRequest> pending_requests_;
};

#endif  // CHROME_BROWSER_UI_WEBUI_DEVTOOLS_DEVTOOLS_UI_DATA_SOURCE_H_
