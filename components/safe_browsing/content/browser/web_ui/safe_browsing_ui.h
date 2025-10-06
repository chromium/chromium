// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAFE_BROWSING_CONTENT_BROWSER_WEB_UI_SAFE_BROWSING_UI_H_
#define COMPONENTS_SAFE_BROWSING_CONTENT_BROWSER_WEB_UI_SAFE_BROWSING_UI_H_

#include <sstream>

#include "components/safe_browsing/core/browser/web_ui/safe_browsing_local_state_delegate.h"
#include "content/public/browser/web_ui_controller.h"

namespace os_crypt_async {
class OSCryptAsync;
}

namespace safe_browsing {

// The WebUI for chrome://safe-browsing
class SafeBrowsingUI : public content::WebUIController {
 protected:
  SafeBrowsingUI(content::WebUI* web_ui,
                 std::unique_ptr<SafeBrowsingLocalStateDelegate> delegate,
                 os_crypt_async::OSCryptAsync* os_crypt_async);

  SafeBrowsingUI(const SafeBrowsingUI&) = delete;
  SafeBrowsingUI& operator=(const SafeBrowsingUI&) = delete;

  ~SafeBrowsingUI() override;
};

// Used for streaming messages to the WebUIContentInfoSingleton. Collects
// streamed messages, then sends them to the WebUIContentInfoSingleton when
// destroyed. Intended to be used in CRSBLOG macro.
class CrSBLogMessage {
 public:
  CrSBLogMessage();
  ~CrSBLogMessage();

  std::ostream& stream() { return stream_; }

 private:
  std::ostringstream stream_;
};

// Used to consume a stream so that we don't even evaluate the streamed data if
// there are no chrome://safe-browsing tabs open.
class CrSBLogVoidify {
 public:
  CrSBLogVoidify() = default;

  // This has to be an operator with a precedence lower than <<,
  // but higher than ?:
  void operator&(std::ostream&) {}
};

#define CRSBLOG                                                               \
  (!::safe_browsing::WebUIContentInfoSingleton::GetInstance()->HasListener()) \
      ? static_cast<void>(0)                                                  \
      : ::safe_browsing::CrSBLogVoidify() &                                   \
            ::safe_browsing::CrSBLogMessage().stream()

}  // namespace safe_browsing

#endif  // COMPONENTS_SAFE_BROWSING_CONTENT_BROWSER_WEB_UI_SAFE_BROWSING_UI_H_
