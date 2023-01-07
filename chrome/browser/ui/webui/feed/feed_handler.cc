// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/feed/feed_handler.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/task/thread_pool.h"

namespace feed {

FeedHandler::FeedHandler(
    mojo::PendingReceiver<feed::mojom::FeedSidePanelHandler> pending_receiver,
    mojo::PendingRemote<feed::mojom::FeedSidePanel> pending_side_panel)
    : receiver_(this, std::move(pending_receiver)),
      side_panel_(std::move(pending_side_panel)) {}

FeedHandler::~FeedHandler() = default;

void FeedHandler::DoSomething() {
  base::ThreadPool::PostTaskAndReply(
      FROM_HERE, base::BindOnce([]() {
        // Do some work here.
      }),
      base::BindOnce(&FeedHandler::OnSomethingDone,
                     weak_ptr_factory_.GetWeakPtr()));
}

void FeedHandler::OnSomethingDone() {
  side_panel_->OnEventOccurred("DoSomething is done");
}

}  // namespace feed
