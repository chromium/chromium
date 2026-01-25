// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/chromedriver/chrome/page_tracker.h"

#include <algorithm>
#include <utility>

#include "base/logging.h"
#include "chrome/test/chromedriver/chrome/devtools_client.h"
#include "chrome/test/chromedriver/chrome/status.h"
#include "chrome/test/chromedriver/chrome/web_view_impl.h"

PageTracker::PageTracker(DevToolsClient* client, WebView* tab_view)
    : tab_view_(tab_view) {
  client->AddListener(this);
}

PageTracker::~PageTracker() = default;

Status PageTracker::OnConnected(DevToolsClient* client) {
  return Status(kOk);
}

Status PageTracker::OnPageCreated(const std::string& session_id,
                                  const std::string& target_id,
                                  WebViewInfo::Type type) {
  WebViewImpl* tab_view = static_cast<WebViewImpl*>(tab_view_);

  if (page_to_target_map_.find(target_id) == page_to_target_map_.end()) {
    std::unique_ptr<WebViewImpl> page_view =
        tab_view->CreatePageWithinTab(session_id, target_id, type);

    WebViewImpl* p = page_view.get();
    page_to_target_map_[target_id] = std::move(page_view);
    tab_view->AttachChildView(p);
  }
  return Status(kOk);
}

Status PageTracker::OnPageActivated(const std::string& target_id) {
  if (page_to_target_map_.find(target_id) == page_to_target_map_.end()) {
    return Status(kOk);
  }

  active_page_ = target_id;
  return Status(kOk);
}

Status PageTracker::OnPageDisconnected(const std::string& target_id) {
  auto it = page_to_target_map_.find(target_id);

  if (it == page_to_target_map_.end()) {
    return Status(kOk);
  }

  WebViewImpl* target = static_cast<WebViewImpl*>(it->second.get());

  if (target->IsLocked()) {
    target->SetDetached();
  } else {
    page_to_target_map_.erase(it);
  }

  if (active_page_ == target_id) {
    active_page_.clear();
  }

  return Status(kOk);
}

void PageTracker::DeletePage(const std::string& page_id) {
  if (page_to_target_map_.find(page_id) == page_to_target_map_.end()) {
    return;
  }
  page_to_target_map_.erase(page_id);
}

Status PageTracker::OnEvent(DevToolsClient* client,
                            const std::string& method,
                            const base::DictValue& params) {
  if (method == "Target.attachedToTarget") {
    const std::string* session_id = params.FindString("sessionId");
    if (!session_id) {
      return Status(kUnknownError,
                    "missing session ID in Target.attachedToTarget event");
    }
    const std::string* target_id =
        params.FindStringByDottedPath("targetInfo.targetId");
    if (!target_id) {
      return Status(kUnknownError,
                    "missing target ID in Target.attachedToTarget event");
    }
    const std::string* target_type =
        params.FindStringByDottedPath("targetInfo.type");
    if (!target_type) {
      return Status(kUnknownError,
                    "missing target type in Target.attachedToTarget event");
    }
    WebViewInfo::Type page_type;
    Status status = WebViewInfo::ParseType(*target_type, page_type);
    if (status.IsError()) {
      return Status(
          kUnknownError,
          "unknown target page type in Target.attachedToTarget event");
    }
    status = OnPageCreated(*session_id, *target_id, page_type);
    if (status.IsError()) {
      return status;
    }

    // See if the page attached is already activated.
    const std::optional<bool> is_attached =
        params.FindBoolByDottedPath("targetInfo.attached");
    const std::string* subtype =
        params.FindStringByDottedPath("targetInfo.subtype");

    if (is_attached.has_value() && is_attached.value() == true &&
        (!subtype || subtype->empty())) {
      status = OnPageActivated(*target_id);
      if (status.IsError()) {
        return status;
      }
    }

  } else if (method == "Target.targetInfoChanged") {
    const std::string* target_id =
        params.FindStringByDottedPath("targetInfo.targetId");
    if (!target_id) {
      return Status(kUnknownError,
                    "missing target ID in Target.attachedToTarget event");
    }

    const std::optional<bool> is_attached =
        params.FindBoolByDottedPath("targetInfo.attached");
    const std::string* subtype =
        params.FindStringByDottedPath("targetInfo.subtype");

    if (is_attached.has_value() && is_attached.value() == true &&
        (!subtype || subtype->empty())) {
      return OnPageActivated(*target_id);
    }
  } else if (method == "Target.detachedFromTarget") {
    const std::string* target_id = params.FindString("targetId");
    if (!target_id) {
      // Some types of Target.detachedFromTarget events do not have targetId.
      // We are not interested in those types of targets.
      return Status(kUnknownError,
                    "missing target ID in Target.detachedFromTarget event");
    }

    return OnPageDisconnected(*target_id);
  }
  return Status(kOk);
}

Status PageTracker::GetActivePage(WebView** web_view) {
  if (!active_page_.empty()) {
    auto it = page_to_target_map_.find(active_page_);
    if (it != page_to_target_map_.end()) {
      *web_view = it->second.get();
      return Status(kOk);
    }
  }
  return Status(kNoActivePage, "active page not found");
}

Status PageTracker::IsPendingActivePage(const Timeout* timeout,
                                        bool* is_pending) {
  *is_pending = active_page_.empty();
  return Status(kOk);
}
