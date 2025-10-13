// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/secure_embed/renderer/secure_embed_web_plugin.h"

#include "base/notimplemented.h"
#include "base/notreached.h"
#include "base/strings/string_number_conversions.h"
#include "cc/paint/paint_canvas.h"
#include "cc/paint/paint_flags.h"
#include "content/public/renderer/render_frame.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "third_party/blink/public/common/input/web_coalesced_input_event.h"
#include "third_party/blink/public/platform/web_input_event_result.h"
#include "third_party/blink/public/platform/web_url_error.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_plugin_container.h"
#include "third_party/blink/public/web/web_plugin_params.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/skia_conversions.h"
#include "v8/include/v8-local-handle.h"

namespace secure_embed {

// static
SecureEmbedWebPlugin* SecureEmbedWebPlugin::Create(
    content::RenderFrame* render_frame,
    const blink::WebPluginParams& params) {
  mojo::AssociatedRemote<mojom::SecureEmbedHost> host;
  render_frame->GetRemoteAssociatedInterfaces()->GetInterface(&host);

  // Read the content ID from the data-content-id attribute
  int contents_id = -1;
  for (size_t i = 0; i < params.attribute_names.size(); ++i) {
    if (params.attribute_names[i].Utf8() == "data-content-id") {
      base::StringToInt(params.attribute_values[i].Utf8(), &contents_id);
      break;
    }
  }

  return new SecureEmbedWebPlugin(std::move(host), contents_id);
}

SecureEmbedWebPlugin::SecureEmbedWebPlugin(
    mojo::AssociatedRemote<mojom::SecureEmbedHost> host,
    int contents_id)
    : contents_id_(contents_id), host_(std::move(host)) {}

SecureEmbedWebPlugin::~SecureEmbedWebPlugin() = default;

bool SecureEmbedWebPlugin::Initialize(blink::WebPluginContainer* container) {
  container_ = container;

  if (host_) {
    mojo::PendingAssociatedRemote<mojom::SecureEmbed> pending_remote =
        receiver_.BindNewEndpointAndPassRemote();
    host_.set_disconnect_handler(
        base::BindOnce(&SecureEmbedWebPlugin::OnSecureEmbedHostDisconnected,
                       base::Unretained(this)));
    receiver_.set_disconnect_handler(
        base::BindOnce(&SecureEmbedWebPlugin::OnSecureEmbedHostDisconnected,
                       base::Unretained(this)));

    // Set up the SecureEmbed interface first.
    host_->SetSecureEmbed(std::move(pending_remote));

    // Then attach with the content ID.
    host_->Attach(contents_id_);
  }
  return true;
}

void SecureEmbedWebPlugin::Destroy() {
  receiver_.reset();
  host_.reset();
  delete this;
}

void SecureEmbedWebPlugin::OnSecureEmbedHostDisconnected() {
  receiver_.reset();
  host_.reset();

  // If the browser side of the connection goes down, we're in an unexpected
  // state and likely need to flag this plugin as broken.
  NOTREACHED();
}

blink::WebPluginContainer* SecureEmbedWebPlugin::Container() const {
  return container_;
}

v8::Local<v8::Object> SecureEmbedWebPlugin::V8ScriptableObject(
    v8::Isolate* isolate) {
  return v8::Local<v8::Object>();
}

void SecureEmbedWebPlugin::UpdateAllLifecyclePhases(
    blink::DocumentUpdateReason reason) {}

void SecureEmbedWebPlugin::Paint(cc::PaintCanvas* canvas,
                                 const gfx::Rect& rect) {
  cc::PaintFlags flags;
  flags.setColor(SK_ColorRED);
  canvas->drawRect(gfx::RectToSkRect(rect), flags);
}

void SecureEmbedWebPlugin::UpdateGeometry(const gfx::Rect& window_rect,
                                          const gfx::Rect& clip_rect,
                                          const gfx::Rect& unobscured_rect,
                                          bool is_visible) {}

void SecureEmbedWebPlugin::UpdateFocus(bool focused,
                                       blink::mojom::FocusType focus_type) {}

void SecureEmbedWebPlugin::UpdateVisibility(bool is_visible) {}

blink::WebInputEventResult SecureEmbedWebPlugin::HandleInputEvent(
    const blink::WebCoalescedInputEvent& event,
    ui::Cursor* cursor) {
  return blink::WebInputEventResult::kNotHandled;
}

void SecureEmbedWebPlugin::DidReceiveResponse(
    const blink::WebURLResponse& response) {
  NOTIMPLEMENTED();
}

void SecureEmbedWebPlugin::DidReceiveData(base::span<const char> data) {
  NOTIMPLEMENTED();
}

void SecureEmbedWebPlugin::DidFinishLoading() {
  NOTIMPLEMENTED();
}

void SecureEmbedWebPlugin::DidFailLoading(const blink::WebURLError& error) {
  NOTIMPLEMENTED();
}

void SecureEmbedWebPlugin::OnAttached() {
  // TODO(secure-embed): Here for testing only. Remove when there's a real
  // SecureEmbed method to implement.
  LOG(INFO) << "SecureEmbedWebPlugin::OnAttached() called";
}

}  // namespace secure_embed
