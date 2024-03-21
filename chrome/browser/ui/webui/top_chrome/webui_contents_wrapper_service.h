// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_TOP_CHROME_WEBUI_CONTENTS_WRAPPER_SERVICE_H_
#define CHROME_BROWSER_UI_WEBUI_TOP_CHROME_WEBUI_CONTENTS_WRAPPER_SERVICE_H_

#include <memory>
#include <string>

#include "base/containers/flat_map.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/webui/top_chrome/webui_contents_wrapper.h"
#include "components/keyed_service/core/keyed_service.h"

class GURL;
class Profile;

// This service maintains a 1-to-1 mapping of WebUIContentsWrapper to WebUI URL
// hostname for a given profile. This allows multiple WebUI bubbles across
// profile windows to share the same WebUIContentsWrapper instance, minimizing
// the resource cost of persisting the wrapped WebContents. Persisting a
// WebContents is done to improve initialization delays for WebUI bubbles.
// A KeyedService is used to ensure any references to the Profile from the
// wrapped WebContents instances are removed before Profile destruction.
class WebUIContentsWrapperService : public KeyedService {
 public:
  explicit WebUIContentsWrapperService(Profile* profile);
  ~WebUIContentsWrapperService() override;

  template <typename T>
  void InitWebUIContentsWrapper(const GURL& webui_url,
                                 int task_manager_string_id) {
    // If replacing an existing WebUIContentsWrapper make sure it has no
    // associated host.
    auto it = web_contents_map_.find(webui_url.host());
    if (it != web_contents_map_.end() && it->second->GetHost()) {
      it->second->CloseUI();
      DCHECK(!it->second->GetHost());
    }

    auto contents_wrapper = std::make_unique<WebUIContentsWrapperT<T>>(
        webui_url, profile_, task_manager_string_id);
    web_contents_map_.insert({webui_url.host(), std::move(contents_wrapper)});
  }

  void Shutdown() override;

  WebUIContentsWrapper* GetWebUIContentsWrapperFromURL(const GURL& webui_url);

 private:
  using WebContentsMap =
      base::flat_map<std::string, std::unique_ptr<WebUIContentsWrapper>>;

  // Profile associated with this service.
  const raw_ptr<Profile> profile_;

  // Associates WebUIContentsWrapper instances with their WebUI URL hostname.
  WebContentsMap web_contents_map_;
};

#endif  // CHROME_BROWSER_UI_WEBUI_TOP_CHROME_WEBUI_CONTENTS_WRAPPER_SERVICE_H_
