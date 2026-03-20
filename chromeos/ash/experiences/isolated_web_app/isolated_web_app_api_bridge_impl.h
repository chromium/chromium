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

namespace views {
class Widget;
}  // namespace views

namespace ash {

// Implements the mojo service for IWA blink extensions in ChromeOS.
class COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_ISOLATED_WEB_APP)
    IsolatedWebAppApiBridgeImpl
    : public content::DocumentUserData<IsolatedWebAppApiBridgeImpl>,
      public blink::mojom::IsolatedWebAppApiBridge {
 public:
  // If the `render_frame_host` is allowed to access this service, this function
  // creates an instance for the document and binds `receiver` to it. Otherwise
  // it does nothing.
  //
  // `render_frame_host` must be non-null and `receiver` must be valid.
  static void Create(
      content::RenderFrameHost* render_frame_host,
      mojo::PendingReceiver<blink::mojom::IsolatedWebAppApiBridge> receiver);

  // Similar to `Create`, but always binds the receiver without without checking
  // that the `render_frame_host` is allowed. Must only be used in tests.
  //
  // `render_frame_host` must be non-null and `receiver` must be valid.
  static void CreateForTesting(
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

  // Binds `receiver` to `receiver_`. If the `receiver_` is already bound,
  // it will be re-bound to the new pipe.
  void Bind(
      mojo::PendingReceiver<blink::mojom::IsolatedWebAppApiBridge> receiver);

  // Returns the top-level `Widget` associated with the `render_frame_host()`,
  // or `nullptr` if a native view cannot be found.
  views::Widget* GetWidget();

  mojo::Receiver<blink::mojom::IsolatedWebAppApiBridge> receiver_{this};

  // When true the API is enabled for every document. Must only be set in tests.
  bool force_enable_api_for_testing_ = false;
};

}  // namespace ash

#endif  // CHROMEOS_ASH_EXPERIENCES_ISOLATED_WEB_APP_ISOLATED_WEB_APP_API_BRIDGE_IMPL_H_
