// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_CRX_DOWNLOADER_FACTORY_H_
#define CHROME_UPDATER_CRX_DOWNLOADER_FACTORY_H_

#include "base/memory/scoped_refptr.h"
#include "components/update_client/crx_downloader_factory.h"

namespace updater {

// Reimplement this function to provide a different download stack for
// downloading CRX payloads.
scoped_refptr<update_client::CrxDownloaderFactory> MakeCrxDownloaderFactory(
    scoped_refptr<update_client::NetworkFetcherFactory>
        network_fetcher_factory) {
  return update_client::MakeCrxDownloaderFactory(network_fetcher_factory);
}

}  // namespace updater

#endif  // CHROME_UPDATER_CRX_DOWNLOADER_FACTORY_H_
