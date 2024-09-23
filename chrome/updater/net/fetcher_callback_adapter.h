// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_NET_FETCHER_CALLBACK_ADAPTER_H_
#define CHROME_UPDATER_NET_FETCHER_CALLBACK_ADAPTER_H_

#include "base/functional/callback.h"
#include "chrome/updater/net/mac/mojom/updater_fetcher.mojom.h"
#include "components/update_client/network.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"

namespace updater {

base::OnceCallback<void(mojo::PendingReceiver<mojom::PostRequestObserver>)>
MakePostRequestObserver(
    update_client::NetworkFetcher::ResponseStartedCallback
        response_started_callback,
    update_client::NetworkFetcher::ProgressCallback progress_callback,
    update_client::NetworkFetcher::PostRequestCompleteCallback
        post_request_complete_callback);

base::OnceCallback<void(mojo::PendingReceiver<mojom::FileDownloadObserver>)>
MakeFileDownloadObserver(
    update_client::NetworkFetcher::ResponseStartedCallback
        response_started_callback,
    update_client::NetworkFetcher::ProgressCallback progress_callback,
    update_client::NetworkFetcher::DownloadToFileCompleteCallback
        download_complete_callback);

}  // namespace updater

#endif  // CHROME_UPDATER_NET_FETCHER_CALLBACK_ADAPTER_H_
