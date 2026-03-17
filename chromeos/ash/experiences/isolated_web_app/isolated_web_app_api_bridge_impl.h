// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_EXPERIENCES_ISOLATED_WEB_APP_ISOLATED_WEB_APP_API_BRIDGE_IMPL_H_
#define CHROMEOS_ASH_EXPERIENCES_ISOLATED_WEB_APP_ISOLATED_WEB_APP_API_BRIDGE_IMPL_H_

#include <vector>

#include "base/component_export.h"
#include "content/public/browser/document_user_data.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "third_party/blink/public/mojom/chromeos/isolated_web_app_api_bridge.mojom.h"
#include "ui/gfx/geometry/rect.h"

namespace content {
class RenderFrameHost;
}  // namespace content

namespace ash {

// Implements the mojo service for IWA blink extensions in ChromeOS.
class COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_ISOLATED_WEB_APP)
    IsolatedWebAppApiBridgeImpl
    : public content::DocumentUserData<IsolatedWebAppApiBridgeImpl>,
      public blink::mojom::IsolatedWebAppApiBridge {
 public:
  // `render_frame_host` and `receiver` must not be null.
  static void Create(
      content::RenderFrameHost* render_frame_host,
      mojo::PendingReceiver<blink::mojom::IsolatedWebAppApiBridge> receiver);

  IsolatedWebAppApiBridgeImpl(const IsolatedWebAppApiBridgeImpl&) = delete;
  IsolatedWebAppApiBridgeImpl& operator=(const IsolatedWebAppApiBridgeImpl&) =
      delete;

  ~IsolatedWebAppApiBridgeImpl() override;

  // blink::mojom::IsolatedWebAppApiBridge:
  void SetShape(const std::vector<gfx::Rect>& rects,
                SetShapeCallback callback) override;

 private:
  friend class content::DocumentUserData<IsolatedWebAppApiBridgeImpl>;
  DOCUMENT_USER_DATA_KEY_DECL();

  explicit IsolatedWebAppApiBridgeImpl(
      content::RenderFrameHost* render_frame_host);

  // Binds the `receiver`. If the receiver is already bound, it will be
  // re-bound to the new pipe.
  void Bind(
      mojo::PendingReceiver<blink::mojom::IsolatedWebAppApiBridge> receiver);

  mojo::Receiver<blink::mojom::IsolatedWebAppApiBridge> receiver_{this};
};

}  // namespace ash

#endif  // CHROMEOS_ASH_EXPERIENCES_ISOLATED_WEB_APP_ISOLATED_WEB_APP_API_BRIDGE_IMPL_H_
