// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/js_injection/browser/js_to_browser_messaging.h"

#include "base/containers/contains.h"
#include "base/memory/raw_ptr.h"
#include "components/js_injection/browser/web_message.h"
#include "components/js_injection/browser/web_message_host.h"
#include "components/js_injection/browser/web_message_host_factory.h"
#include "components/js_injection/browser/web_message_reply_proxy.h"
#include "components/js_injection/common/interfaces.mojom-forward.h"
#include "content/public/browser/disallow_activation_reason.h"
#include "content/public/browser/document_user_data.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "third_party/blink/public/common/messaging/message_port_descriptor.h"
#include "third_party/blink/public/common/messaging/string_message_codec.h"
#include "url/origin.h"
#include "url/url_util.h"

namespace js_injection {
namespace {

// We want to pass a string "null" for local file schemes, to make it
// consistent to the Blink side SecurityOrigin serialization. When both
// setAllow{File,Universal}AccessFromFileURLs are false, Blink::SecurityOrigin
// will be serialized as string "null" for local file schemes, but when
// setAllowFileAccessFromFileURLs is true, Blink::SecurityOrigin will be
// serialized as the scheme, which will be inconsistentt to this place. In
// this case we want to let developer to know that local files are not safe,
// so we still pass "null".
std::string GetOriginString(const url::Origin& source_origin) {
  return base::Contains(url::GetLocalSchemes(), source_origin.scheme())
             ? "null"
             : source_origin.Serialize();
}

// Used for queueing messages posted during prerendering. DocumentUserData
// should be appropriate for managing them as the messages should be discarded
// when an associated document is gone.
class JsToBrowserMessagingDocumentUserData
    : public content::DocumentUserData<JsToBrowserMessagingDocumentUserData> {
 public:
  ~JsToBrowserMessagingDocumentUserData() override = default;

  std::vector<std::unique_ptr<WebMessage>>& queued_messages() {
    return queued_messages_;
  }

 private:
  friend class DocumentUserData<JsToBrowserMessagingDocumentUserData>;

  explicit JsToBrowserMessagingDocumentUserData(
      content::RenderFrameHost* render_frame_host)
      : content::DocumentUserData<JsToBrowserMessagingDocumentUserData>(
            render_frame_host) {}

  std::vector<std::unique_ptr<WebMessage>> queued_messages_;

  DOCUMENT_USER_DATA_KEY_DECL();
};

DOCUMENT_USER_DATA_KEY_IMPL(JsToBrowserMessagingDocumentUserData);

}  // namespace

class JsToBrowserMessaging::ReplyProxyImpl : public WebMessageReplyProxy {
 public:
  ReplyProxyImpl(
      content::RenderFrameHost* render_frame_host,
      mojo::PendingAssociatedRemote<mojom::BrowserToJsMessaging>
          java_to_js_messaging,
      mojo::SharedAssociatedRemote<mojom::BrowserToJsMessagingFactory> factory)
      : render_frame_host_(render_frame_host),
        java_to_js_messaging_(std::move(java_to_js_messaging)),
        factory_(std::move(factory)) {}
  ReplyProxyImpl(const ReplyProxyImpl&) = delete;
  ReplyProxyImpl& operator=(const ReplyProxyImpl&) = delete;
  ~ReplyProxyImpl() override = default;

  // WebMessageReplyProxy:
  void PostWebMessage(blink::WebMessagePayload message) override {
    EnsureBrowserToJsMessaging();
    java_to_js_messaging_->OnPostMessage(std::move(message));
  }

  void EnsureBrowserToJsMessaging() {
    if (!java_to_js_messaging_ && factory_) {
      factory_->SendBrowserToJsMessaging(
          java_to_js_messaging_.BindNewEndpointAndPassReceiver());
    }
  }

  content::Page& GetPage() override { return render_frame_host_->GetPage(); }

 private:
  raw_ptr<content::RenderFrameHost> render_frame_host_;
  mojo::AssociatedRemote<mojom::BrowserToJsMessaging> java_to_js_messaging_;
  mojo::SharedAssociatedRemote<mojom::BrowserToJsMessagingFactory> factory_;
};

JsToBrowserMessaging::JsToBrowserMessaging(
    content::RenderFrameHost* render_frame_host,
    mojo::PendingAssociatedReceiver<mojom::JsToBrowserMessaging> receiver,
    mojo::PendingAssociatedRemote<mojom::BrowserToJsMessagingFactory>
        browser_to_js_factory,
    WebMessageHostFactory* factory,
    const OriginMatcher& origin_matcher)
    : render_frame_host_(render_frame_host),
      connection_factory_(factory),
      origin_matcher_(origin_matcher),
      browser_to_js_factory_(std::move(browser_to_js_factory)) {
  receiver_.Bind(std::move(receiver));
}

JsToBrowserMessaging::~JsToBrowserMessaging() = default;

void JsToBrowserMessaging::OnRenderFrameHostActivated() {
  JsToBrowserMessagingDocumentUserData* data =
      JsToBrowserMessagingDocumentUserData::GetForCurrentDocument(
          render_frame_host_);
  if (!data) {
    return;
  }

  if (!host_) {
    return;
  }

  for (auto& message : data->queued_messages()) {
    host_->OnPostMessage(std::move(message));
  }
  data->queued_messages().clear();
}

void JsToBrowserMessaging::PostMessage(
    blink::WebMessagePayload message,
    std::vector<blink::MessagePortDescriptor> ports) {
  DCHECK(render_frame_host_);

  // For prerendering, messages will be queued until activation.
  if (!render_frame_host_->IsInLifecycleState(
          content::RenderFrameHost::LifecycleState::kPrerendering) &&
      render_frame_host_->IsInactiveAndDisallowActivation(
          content::DisallowActivationReasonId::kJsInjectionPostMessage)) {
    return;
  }

  content::WebContents* web_contents =
      content::WebContents::FromRenderFrameHost(render_frame_host_);

  if (!web_contents)
    return;

  const url::Origin top_level_origin =
      render_frame_host_->GetMainFrame()->GetLastCommittedOrigin();
  // |source_origin| has no race with this PostMessage call, because of
  // associated mojo channel, the committed origin message and PostMessage are
  // in sequence.
  const url::Origin source_origin =
      render_frame_host_->GetLastCommittedOrigin();

  if (!origin_matcher_.Matches(source_origin))
    return;

  // SetBrowserToJsMessaging must be called before this.
  DCHECK(reply_proxy_);
  reply_proxy_->EnsureBrowserToJsMessaging();

  if (!host_) {
    const std::string top_level_origin_string =
        GetOriginString(top_level_origin);
    const std::string origin_string = GetOriginString(source_origin);

    // Check if this is the main frame of the primary or prerendered page.
    const bool is_main_frame = !render_frame_host_->GetParentOrOuterDocument();

    host_ =
        connection_factory_->CreateHost(top_level_origin_string, origin_string,
                                        is_main_frame, reply_proxy_.get());
#if DCHECK_IS_ON()
    top_level_origin_string_ = top_level_origin_string;
    origin_string_ = origin_string;
    is_main_frame_ = is_main_frame;
#endif
    if (!host_)
      return;
  }
  // The origin and whether this is the main frame should not change once
  // PostMessage() has been received.
#if DCHECK_IS_ON()
  DCHECK_EQ(GetOriginString(top_level_origin), top_level_origin_string_);
  DCHECK_EQ(GetOriginString(source_origin), origin_string_);
  DCHECK_EQ(is_main_frame_, !render_frame_host_->GetParentOrOuterDocument());
#endif
  std::unique_ptr<WebMessage> web_message = std::make_unique<WebMessage>();
  web_message->message = std::move(message);
  web_message->ports = std::move(ports);

  if (render_frame_host_->IsInLifecycleState(
          content::RenderFrameHost::LifecycleState::kPrerendering)) {
    // Queue `WebMessage`s received while prerendering. They are flushed on
    // `OnRenderFrameHostActivated`.
    JsToBrowserMessagingDocumentUserData* data =
        JsToBrowserMessagingDocumentUserData::GetOrCreateForCurrentDocument(
            render_frame_host_);
    data->queued_messages().push_back(std::move(web_message));
    return;
  }

  host_->OnPostMessage(std::move(web_message));
}

void JsToBrowserMessaging::SetBrowserToJsMessaging(
    mojo::PendingAssociatedRemote<mojom::BrowserToJsMessaging>
        java_to_js_messaging) {
  // TODO(crbug.com/40752101): this should really call
  // IsInactiveAndDisallowReactivation().

  // A RenderFrame may inject JsToBrowserMessaging in the JavaScript context
  // more than once because of reusing of RenderFrame.
  host_.reset();
  reply_proxy_ = std::make_unique<ReplyProxyImpl>(
      render_frame_host_, std::move(java_to_js_messaging),
      browser_to_js_factory_);
}

}  // namespace js_injection
