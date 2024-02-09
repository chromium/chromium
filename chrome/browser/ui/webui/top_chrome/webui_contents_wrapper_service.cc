// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/top_chrome/webui_contents_wrapper_service.h"

#include "chrome/browser/profiles/profile.h"
#include "url/gurl.h"

WebUIContentsWrapperService::WebUIContentsWrapperService(Profile* profile)
    : profile_(profile) {}

WebUIContentsWrapperService::~WebUIContentsWrapperService() = default;

void WebUIContentsWrapperService::Shutdown() {
  for (auto& webui_contents : web_contents_map_) {
    webui_contents.second->CloseUI();
    DCHECK(!webui_contents.second->GetHost());
  }
  web_contents_map_.clear();
}

WebUIContentsWrapper*
WebUIContentsWrapperService::GetWebUIContentsWrapperFromURL(
    const GURL& webui_url) {
  auto it = web_contents_map_.find(webui_url.host());
  return it == web_contents_map_.end() ? nullptr : it->second.get();
}
