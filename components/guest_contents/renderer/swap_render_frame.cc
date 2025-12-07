// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/guest_contents/renderer/swap_render_frame.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/supports_user_data.h"
#include "components/guest_contents/common/guest_contents.mojom.h"
#include "content/public/renderer/render_frame.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/public/platform/browser_interface_broker_proxy.h"
#include "third_party/blink/public/web/web_local_frame.h"

namespace guest_contents::renderer {

namespace {

const void* const kGuestContentsServiceKey = &kGuestContentsServiceKey;

// A service that holds a connection to the GuestContentsHost in the browser
// process. This service is available on the main frame.
class GuestContentsService : public base::SupportsUserData::Data {
 public:
  explicit GuestContentsService(content::RenderFrame* render_frame) {
    // GuestContentsHost is available on the WebUIController owned by the main
    // frame.
    CHECK(render_frame->IsMainFrame());
    render_frame->GetBrowserInterfaceBroker().GetInterface(
        remote_.BindNewPipeAndPassReceiver());
  }

  static GuestContentsService* GetOrCreateForRenderFrame(
      content::RenderFrame* render_frame) {
    if (!render_frame->GetUserData(kGuestContentsServiceKey)) {
      render_frame->SetUserData(
          kGuestContentsServiceKey,
          std::make_unique<GuestContentsService>(render_frame));
    }

    return static_cast<GuestContentsService*>(
        render_frame->GetUserData(kGuestContentsServiceKey));
  }

  mojo::Remote<mojom::GuestContentsHost>& remote() { return remote_; }

 private:
  mojo::Remote<mojom::GuestContentsHost> remote_;
};

}  // namespace

void SwapRenderFrame(content::RenderFrame* render_frame,
                     int guest_contents_id) {
  // `render_frame` is the outer delegate frame. It must be in the same process
  // as the main frame. If not this CHECK will fail.
  CHECK(render_frame->GetMainRenderFrame());
  GuestContentsService::GetOrCreateForRenderFrame(
      render_frame->GetMainRenderFrame())
      ->remote()
      ->Attach(render_frame->GetWebFrame()->GetLocalFrameToken(),
               guest_contents_id,
               base::BindOnce([](bool success) { CHECK(success); }));
}

}  // namespace guest_contents::renderer
