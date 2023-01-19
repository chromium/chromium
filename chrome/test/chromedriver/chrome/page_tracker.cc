// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/chromedriver/chrome/page_tracker.h"

#include <utility>

#include "base/logging.h"
#include "base/ranges/algorithm.h"
#include "chrome/test/chromedriver/chrome/devtools_client.h"
#include "chrome/test/chromedriver/chrome/status.h"
#include "chrome/test/chromedriver/chrome/web_view_impl.h"

PageTracker::PageTracker(DevToolsClient* client,
                         std::list<std::unique_ptr<WebViewImpl>>* web_views)
    : web_views_(web_views) {
  client->AddListener(this);
}

PageTracker::~PageTracker() = default;

bool PageTracker::ListensToConnections() const {
  return false;
}

Status PageTracker::OnEvent(DevToolsClient* client,
                            const std::string& method,
                            const base::Value::Dict& params) {
  if (method == "Target.detachedFromTarget") {
    const std::string* target_id = params.FindString("targetId");
    if (!target_id)
      // Some types of Target.detachedFromTarget events do not have targetId.
      // We are not interested in those types of targets.
      return Status(kOk);
    auto it = base::ranges::find(*web_views_, *target_id, &WebViewImpl::GetId);
    if (it == web_views_->end()) {
      // There are some target types that we're not keeping track of, thus not
      // finding the target in frame_to_target_map_ is OK.
      return Status(kOk);
    }
    WebViewImpl* target = static_cast<WebViewImpl*>(it->get());
    if (target->IsLocked()) {
      target->SetDetached();
    } else {
      web_views_->erase(it);
    }
  }
  return Status(kOk);
}
