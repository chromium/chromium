// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_JS_INJECTION_BROWSER_JS_TO_BROWSER_MESSAGING_H_
#define COMPONENTS_JS_INJECTION_BROWSER_JS_TO_BROWSER_MESSAGING_H_

#include <vector>

#include "base/check.h"
#include "base/strings/string16.h"
#include "components/js_injection/common/interfaces.mojom.h"
#include "components/js_injection/common/origin_matcher.h"
#include "mojo/public/cpp/bindings/associated_receiver_set.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_associated_remote.h"
#include "third_party/blink/public/common/messaging/message_port_descriptor.h"

namespace content {
class RenderFrameHost;
}

namespace js_injection {

class WebMessageHost;
class WebMessageHostFactory;

// Implementation of mojo::JsToBrowserMessaging interface. Receives
// PostMessage() call from renderer JsBinding.
class JsToBrowserMessaging : public mojom::JsToBrowserMessaging {
 public:
  JsToBrowserMessaging(
      content::RenderFrameHost* rfh,
      mojo::PendingAssociatedReceiver<mojom::JsToBrowserMessaging> receiver,
      WebMessageHostFactory* factory,
      const OriginMatcher& origin_matcher);
  ~JsToBrowserMessaging() override;

  // mojom::JsToBrowserMessaging implementation.
  void PostMessage(const base::string16& message,
                   std::vector<blink::MessagePortDescriptor> ports) override;
  void SetBrowserToJsMessaging(
      mojo::PendingAssociatedRemote<mojom::BrowserToJsMessaging>
          java_to_js_messaging) override;

 private:
  class ReplyProxyImpl;

  content::RenderFrameHost* render_frame_host_;
  std::unique_ptr<ReplyProxyImpl> reply_proxy_;
  WebMessageHostFactory* connection_factory_;
  OriginMatcher origin_matcher_;
  mojo::AssociatedReceiver<mojom::JsToBrowserMessaging> receiver_{this};
  std::unique_ptr<WebMessageHost> host_;
#if DCHECK_IS_ON()
  std::string origin_string_;
  bool is_main_frame_;
#endif

  DISALLOW_COPY_AND_ASSIGN(JsToBrowserMessaging);
};

}  // namespace js_injection

#endif  // COMPONENTS_JS_INJECTION_BROWSER_JS_TO_BROWSER_MESSAGING_H_
