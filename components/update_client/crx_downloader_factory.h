// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_UPDATE_CLIENT_CRX_DOWNLOADER_FACTORY_H_
#define COMPONENTS_UPDATE_CLIENT_CRX_DOWNLOADER_FACTORY_H_

#include <cstdint>
#include <optional>

#include "base/files/file_path.h"
#include "base/memory/ref_counted.h"

namespace update_client {

class CrxDownloader;
class NetworkFetcherFactory;

// Creates instances of |CrxDownloader|. Callers of update client can implement
// a factory that provides a different download stack for CRXs. Currently,
// the factory is injected using a |Configurator| function.
class CrxDownloaderFactory
    : public base::RefCountedThreadSafe<CrxDownloaderFactory> {
 public:
  CrxDownloaderFactory(const CrxDownloaderFactory&) = delete;
  CrxDownloaderFactory& operator=(const CrxDownloaderFactory&) = delete;

  virtual scoped_refptr<CrxDownloader> MakeCrxDownloader(
      bool background_download_enabled) const = 0;

 protected:
  friend class base::RefCountedThreadSafe<CrxDownloaderFactory>;

  CrxDownloaderFactory() = default;
  virtual ~CrxDownloaderFactory() = default;
};

scoped_refptr<CrxDownloaderFactory> MakeCrxDownloaderFactory(
    scoped_refptr<NetworkFetcherFactory> network_fetcher_factory,
    std::optional<base::FilePath> background_downloader_cache_path =
        std::nullopt);

}  // namespace update_client

#endif  // COMPONENTS_UPDATE_CLIENT_CRX_DOWNLOADER_FACTORY_H_
