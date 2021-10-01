// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_WHATS_NEW_WHATS_NEW_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_WHATS_NEW_WHATS_NEW_HANDLER_H_

#include <unordered_map>

#include "base/memory/weak_ptr.h"
#include "content/public/browser/web_ui_message_handler.h"

namespace base {
class ListValue;
}

namespace network {
class SimpleURLLoader;
}

// Page handler for chrome://whats-new.
class WhatsNewHandler : public content::WebUIMessageHandler {
 public:
  WhatsNewHandler();
  ~WhatsNewHandler() override;
  WhatsNewHandler(const WhatsNewHandler&) = delete;
  WhatsNewHandler& operator=(const WhatsNewHandler&) = delete;

 private:
  void HandleInitialize(const base::ListValue* args);
  typedef base::OnceCallback<void(bool success,
                                  bool page_not_found,
                                  std::unique_ptr<std::string> body)>
      OnFetchResultCallback;
  void Fetch(const GURL& url, OnFetchResultCallback on_result);
  void OnResponseLoaded(const network::SimpleURLLoader* loader,
                        OnFetchResultCallback on_result,
                        std::unique_ptr<std::string> body);
  void OnFetchResult(const std::string& callback_id,
                     bool is_auto,
                     bool success,
                     bool page_not_found,
                     std::unique_ptr<std::string> body);

  // content::WebUIMessageHandler:
  void RegisterMessages() override;
  void OnJavascriptAllowed() override;
  void OnJavascriptDisallowed() override;

  int num_retries_ = 0;
  std::unordered_map<const network::SimpleURLLoader*,
                     std::unique_ptr<network::SimpleURLLoader>>
      loader_map_;
  base::WeakPtrFactory<WhatsNewHandler> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_WEBUI_WHATS_NEW_WHATS_NEW_HANDLER_H_
