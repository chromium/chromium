// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_JS_INJECTION_BROWSER_WEB_MESSAGE_HOST_FACTORY_H_
#define COMPONENTS_JS_INJECTION_BROWSER_WEB_MESSAGE_HOST_FACTORY_H_

#include <memory>
#include <string>

namespace js_injection {

class WebMessageHost;
class WebMessageReplyProxy;

// Creates a WebMessageHost in response to a page interacting with the object
// registered by way of JsCommunicationHost::AddWebMessageHostFactory(). A
// WebMessageHost is created for every page that matches the parameters of
// AddWebMessageHostFactory().
class WebMessageHostFactory {
 public:
  virtual ~WebMessageHostFactory() = default;

  // Creates a WebMessageHost for the specified page. |proxy| is valid for
  // the life of the host and may be used to send messages back to the page.
  virtual std::unique_ptr<WebMessageHost> CreateHost(
      const std::string& top_level_origin_string,
      const std::string& origin_string,
      bool is_main_frame,
      WebMessageReplyProxy* proxy) = 0;
};

}  // namespace js_injection

#endif  // COMPONENTS_JS_INJECTION_BROWSER_WEB_MESSAGE_HOST_FACTORY_H_
