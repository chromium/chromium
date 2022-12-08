// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DOWNLOAD_PUBLIC_COMMON_URL_DOWNLOAD_HANDLER_H_
#define COMPONENTS_DOWNLOAD_PUBLIC_COMMON_URL_DOWNLOAD_HANDLER_H_

#include "components/download/public/common/download_export.h"
#include "components/download/public/common/download_url_parameters.h"
#include "components/download/public/common/url_loader_factory_provider.h"

namespace download {
struct DownloadCreateInfo;
class InputStream;

// Identifier for a UrlDownloadHandler to scope the lifetime of references.
// UrlDownloadHandlerID are derived from UrlDownloadHandler*, used in
// comparison only, and are never dereferenced. We use an std::uintptr_t here to
// match the size of a pointer, and to prevent dereferencing. Also, our
// tooling complains about dangling pointers if we pass around a raw ptr.
using UrlDownloadHandlerID = std::uintptr_t;

// Class for handling the download of a url. Implemented by child classes.
class COMPONENTS_DOWNLOAD_EXPORT UrlDownloadHandler {
 public:
  using UniqueUrlDownloadHandlerPtr =
      std::unique_ptr<UrlDownloadHandler, base::OnTaskRunnerDeleter>;
  // Class to be notified when download starts/stops.
  class COMPONENTS_DOWNLOAD_EXPORT Delegate {
   public:
    virtual void OnUrlDownloadStarted(
        std::unique_ptr<DownloadCreateInfo> download_create_info,
        std::unique_ptr<InputStream> input_stream,
        URLLoaderFactoryProvider::URLLoaderFactoryProviderPtr
            url_loader_factory_provider,
        UrlDownloadHandlerID downloader,
        DownloadUrlParameters::OnStartedCallback callback) = 0;

    // Called after the connection is cancelled or finished.
    virtual void OnUrlDownloadStopped(UrlDownloadHandlerID downloader) = 0;

    // Called when a UrlDownloadHandler is created.
    virtual void OnUrlDownloadHandlerCreated(
        UniqueUrlDownloadHandlerPtr downloader) {}
  };

  UrlDownloadHandler() = default;

  UrlDownloadHandler(const UrlDownloadHandler&) = delete;
  UrlDownloadHandler& operator=(const UrlDownloadHandler&) = delete;

  virtual ~UrlDownloadHandler() = default;
};

}  // namespace download

#endif  // COMPONENTS_DOWNLOAD_PUBLIC_COMMON_URL_DOWNLOAD_HANDLER_H_
