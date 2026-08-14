// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_EXPERIENCES_ISOLATED_WEB_APP_SET_SHAPE_SERVICE_IMPL_H_
#define CHROMEOS_ASH_EXPERIENCES_ISOLATED_WEB_APP_SET_SHAPE_SERVICE_IMPL_H_

#include <optional>
#include <vector>

#include "base/component_export.h"
#include "content/public/browser/document_user_data.h"
#include "content/public/browser/permission_controller.h"
#include "content/public/browser/permission_result.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "third_party/blink/public/mojom/set_shape/set_shape.mojom.h"
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
    SetShapeServiceImpl : public content::DocumentUserData<SetShapeServiceImpl>,
                          public blink::mojom::SetShapeService {
 public:
  // If the `render_frame_host` is allowed to access this service, this function
  // creates an instance for the document and binds `receiver` to it. Otherwise
  // it does nothing.
  //
  // `render_frame_host` must be non-null and `receiver` must be valid.
  static void Create(
      content::RenderFrameHost* render_frame_host,
      mojo::PendingReceiver<blink::mojom::SetShapeService> receiver);

  // Similar to `Create`, but always binds the receiver without without checking
  // that the `render_frame_host` is allowed. Must only be used in tests.
  //
  // `render_frame_host` must be non-null and `receiver` must be valid.
  static void CreateForTesting(
      content::RenderFrameHost* render_frame_host,
      mojo::PendingReceiver<blink::mojom::SetShapeService> receiver);

  SetShapeServiceImpl(const SetShapeServiceImpl&) = delete;
  SetShapeServiceImpl& operator=(const SetShapeServiceImpl&) = delete;

  ~SetShapeServiceImpl() override;

  // blink::mojom::SetShapeService:
  void SetShape(const std::vector<gfx::Rect>& rects,
                SetShapeCallback callback) override;

 private:
  friend class content::DocumentUserData<SetShapeServiceImpl>;
  DOCUMENT_USER_DATA_KEY_DECL();

  explicit SetShapeServiceImpl(content::RenderFrameHost* render_frame_host);

  // Resets any custom shape and event targeter in the window back to default.
  void ResetShape();

  // Callback triggered when WINDOW_MANAGEMENT permission changes.
  void OnWindowManagementPermissionChanged(content::PermissionResult result);

  // Subscribes to WINDOW_MANAGEMENT permission changes.
  void SubscribeToWindowManagementPermissionChanges();

  // Unsubscribes from WINDOW_MANAGEMENT permission changes.
  void UnsubscribeFromWindowManagementPermissionChanges();

  // Binds `receiver` to `receiver_`. If the `receiver_` is already bound,
  // it will be re-bound to the new pipe.
  void Bind(mojo::PendingReceiver<blink::mojom::SetShapeService> receiver);

  // Returns the top-level `Widget` associated with the `render_frame_host()`,
  // or `nullptr` if a native view cannot be found.
  views::Widget* GetWidget();

  mojo::Receiver<blink::mojom::SetShapeService> receiver_{this};

  // The ID set when a WINDOW_MANAGEMENT subscription is active.
  std::optional<content::PermissionController::SubscriptionId>
      permission_subscription_id_;

  // When true the API is enabled for every document. Must only be set in tests.
  bool force_enable_api_for_testing_ = false;
};

}  // namespace ash

#endif  // CHROMEOS_ASH_EXPERIENCES_ISOLATED_WEB_APP_SET_SHAPE_SERVICE_IMPL_H_
