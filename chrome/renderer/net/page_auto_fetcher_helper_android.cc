// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/net/page_auto_fetcher_helper_android.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "content/public/renderer/render_frame.h"
#include "content/public/renderer/render_thread.h"
#include "third_party/blink/public/platform/browser_interface_broker_proxy.h"

PageAutoFetcherHelper::PageAutoFetcherHelper(content::RenderFrame* render_frame)
    : render_frame_(render_frame) {}
PageAutoFetcherHelper::~PageAutoFetcherHelper() = default;

void PageAutoFetcherHelper::OnCommitLoad() {
  // Make sure we don't try to re-use the same mojo interface for more than one
  // page. Otherwise, the browser side will use the old page's URL.
  fetcher_.reset();
}

void PageAutoFetcherHelper::TrySchedule(
    bool user_requested,
    base::OnceCallback<void(FetcherScheduleResult)> complete_callback) {
  if (!Bind()) {
    std::move(complete_callback).Run(FetcherScheduleResult::kOtherError);
    return;
  }

  fetcher_->TrySchedule(
      user_requested,
      base::BindOnce(&PageAutoFetcherHelper::TryScheduleComplete,
                     weak_ptr_factory_.GetWeakPtr(),
                     std::move(complete_callback)));
}

void PageAutoFetcherHelper::TryScheduleComplete(
    base::OnceCallback<void(FetcherScheduleResult)> complete_callback,
    FetcherScheduleResult result) {
  std::move(complete_callback).Run(result);
}

void PageAutoFetcherHelper::CancelSchedule() {
  if (Bind()) {
    fetcher_->CancelSchedule();
  }
}

bool PageAutoFetcherHelper::Bind() {
  if (fetcher_)
    return true;
  render_frame_->GetBrowserInterfaceBroker().GetInterface(
      fetcher_.BindNewPipeAndPassReceiver());
  return fetcher_.is_bound();
}
