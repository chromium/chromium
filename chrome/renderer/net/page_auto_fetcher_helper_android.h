// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_NET_PAGE_AUTO_FETCHER_HELPER_ANDROID_H_
#define CHROME_RENDERER_NET_PAGE_AUTO_FETCHER_HELPER_ANDROID_H_

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/common/offline_page_auto_fetcher.mojom.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace content {
class RenderFrame;
}

// Wraps calls from the renderer thread to the PageAutoFetcher, and records
// related UMA.
class PageAutoFetcherHelper {
 public:
  using FetcherScheduleResult =
      chrome::mojom::OfflinePageAutoFetcherScheduleResult;
  explicit PageAutoFetcherHelper(content::RenderFrame* render_frame);

  PageAutoFetcherHelper(const PageAutoFetcherHelper&) = delete;
  PageAutoFetcherHelper& operator=(const PageAutoFetcherHelper&) = delete;

  virtual ~PageAutoFetcherHelper();
  // Should be called for each page load.
  void OnCommitLoad();
  void TrySchedule(
      bool user_requested,
      base::OnceCallback<void(FetcherScheduleResult)> complete_callback);
  void CancelSchedule();

 protected:
  void TryScheduleComplete(
      base::OnceCallback<void(FetcherScheduleResult)> complete_callback,
      FetcherScheduleResult result);

  // Binds |fetcher_| if necessary. Returns true if the fetcher_ is bound.
  // Virtual for testing only.
  virtual bool Bind();

  raw_ptr<content::RenderFrame> render_frame_;
  mojo::Remote<chrome::mojom::OfflinePageAutoFetcher> fetcher_;

  base::WeakPtrFactory<PageAutoFetcherHelper> weak_ptr_factory_{this};
};

#endif  // CHROME_RENDERER_NET_PAGE_AUTO_FETCHER_HELPER_ANDROID_H_
