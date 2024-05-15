// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_JS_INJECTION_BROWSER_JS_TO_BROWSER_MESSAGING_H_
#define COMPONENTS_JS_INJECTION_BROWSER_JS_TO_BROWSER_MESSAGING_H_

#include <string>
#include <vector>

#include "base/check.h"
#include "base/memory/raw_ptr.h"
#include "components/js_injection/common/interfaces.mojom.h"
#include "components/js_injection/common/origin_matcher.h"
#include "mojo/public/cpp/bindings/associated_receiver_set.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_associated_remote.h"
#include "mojo/public/cpp/bindings/shared_associated_remote.h"
#include "third_party/blink/public/common/messaging/message_port_descriptor.h"
#include "third_party/blink/public/common/messaging/string_message_codec.h"

namespace content {
class RenderFrameHost;
}

namespace js_injection {

class WebMessageHost;
class WebMessageHostFactory;

// Implementation of mojo::JsToBrowserMessaging interface. Receives
// PostMessage() call from renderer JsBinding.
//
// This object is destroyed when the associated RenderFrameHost is destroyed.
class JsToBrowserMessaging : public mojom::JsToBrowserMessaging {
 public:
  JsToBrowserMessaging(
      content::RenderFrameHost* rfh,
      mojo::PendingAssociatedReceiver<mojom::JsToBrowserMessaging> receiver,
      mojo::PendingAssociatedRemote<mojom::BrowserToJsMessagingFactory>
          browser_to_js_factory,
      WebMessageHostFactory* factory,
      const OriginMatcher& origin_matcher);

  JsToBrowserMessaging(const JsToBrowserMessaging&) = delete;
  JsToBrowserMessaging& operator=(const JsToBrowserMessaging&) = delete;

  ~JsToBrowserMessaging() override;

  void OnRenderFrameHostActivated();

  // mojom::JsToBrowserMessaging implementation.
  void PostMessage(blink::WebMessagePayload message,
                   std::vector<blink::MessagePortDescriptor> ports) override;
  void SetBrowserToJsMessaging(
      mojo::PendingAssociatedRemote<mojom::BrowserToJsMessaging>
          java_to_js_messaging) override;

 private:
  class ReplyProxyImpl;

  const raw_ptr<content::RenderFrameHost> render_frame_host_;
  std::unique_ptr<ReplyProxyImpl> reply_proxy_;
  raw_ptr<WebMessageHostFactory, AcrossTasksDanglingUntriaged>
      connection_factory_;
  OriginMatcher origin_matcher_;
  mojo::AssociatedReceiver<mojom::JsToBrowserMessaging> receiver_{this};
  std::unique_ptr<WebMessageHost> host_;
  mojo::SharedAssociatedRemote<mojom::BrowserToJsMessagingFactory>
      browser_to_js_factory_;
#if DCHECK_IS_ON()
  std::string top_level_origin_string_;
  std::string origin_string_;
  bool is_main_frame_;
#endif
};

}  // namespace js_injection

#endif  // COMPONENTS_JS_INJECTION_BROWSER_JS_TO_BROWSER_MESSAGING_H_
