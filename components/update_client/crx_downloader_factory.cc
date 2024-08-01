// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/update_client/crx_downloader_factory.h"

#include <cstdint>
#include <optional>

#include "base/files/file_path.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "build/build_config.h"
#include "components/update_client/crx_downloader.h"
#include "components/update_client/network.h"
#include "components/update_client/task_traits.h"
#include "components/update_client/url_fetcher_downloader.h"

#if BUILDFLAG(IS_WIN)
#include "components/update_client/background_downloader_win.h"
#elif BUILDFLAG(IS_MAC)
#include "components/update_client/background_downloader_mac.h"
#endif

namespace update_client {
namespace {

class CrxDownloaderFactoryChromium : public CrxDownloaderFactory {
 public:
  explicit CrxDownloaderFactoryChromium(
      scoped_refptr<NetworkFetcherFactory> network_fetcher_factory,
      std::optional<base::FilePath> background_downloader_cache_path)
      : network_fetcher_factory_(network_fetcher_factory) {
#if BUILDFLAG(IS_MAC)
    if (background_downloader_cache_path) {
      background_sequence_ = base::ThreadPool::CreateSequencedTaskRunner(
          kTaskTraitsBackgroundDownloader);
      background_downloader_shared_session_ =
          MakeBackgroundDownloaderSharedSession(
              background_sequence_, *background_downloader_cache_path);
    }
#endif
  }

  // Overrides for CrxDownloaderFactory.
  scoped_refptr<CrxDownloader> MakeCrxDownloader(
      bool background_download_enabled) const override;

 private:
  ~CrxDownloaderFactoryChromium() override = default;

  scoped_refptr<NetworkFetcherFactory> network_fetcher_factory_;
#if BUILDFLAG(IS_MAC)
  scoped_refptr<base::SequencedTaskRunner> background_sequence_;
  scoped_refptr<BackgroundDownloaderSharedSession>
      background_downloader_shared_session_;
#endif
};

scoped_refptr<CrxDownloader> CrxDownloaderFactoryChromium::MakeCrxDownloader(
    bool background_download_enabled) const {
  scoped_refptr<CrxDownloader> url_fetcher_downloader =
      base::MakeRefCounted<UrlFetcherDownloader>(nullptr,
                                                 network_fetcher_factory_);
  if (background_download_enabled) {
#if BUILDFLAG(IS_MAC)
    return base::MakeRefCounted<BackgroundDownloader>(
        url_fetcher_downloader, background_downloader_shared_session_,
        background_sequence_);
#elif BUILDFLAG(IS_WIN)
    return base::MakeRefCounted<BackgroundDownloader>(url_fetcher_downloader);
#endif
  }

  return url_fetcher_downloader;
}

}  // namespace

scoped_refptr<CrxDownloaderFactory> MakeCrxDownloaderFactory(
    scoped_refptr<NetworkFetcherFactory> network_fetcher_factory,
    std::optional<base::FilePath> background_downloader_cache_path) {
  return base::MakeRefCounted<CrxDownloaderFactoryChromium>(
      network_fetcher_factory, background_downloader_cache_path);
}

}  // namespace update_client
