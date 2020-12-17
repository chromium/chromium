// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_CAMERA_APP_UI_CAMERA_APP_UI_H_
#define CHROMEOS_COMPONENTS_CAMERA_APP_UI_CAMERA_APP_UI_H_

#include "base/macros.h"
#include "chromeos/components/camera_app_ui/camera_app_helper.mojom.h"
#include "chromeos/components/camera_app_ui/camera_app_ui_delegate.h"
#include "chromeos/components/camera_app_ui/camera_app_window_manager.h"
#include "content/public/browser/devtools_agent_host_observer.h"
#include "content/public/browser/web_ui.h"
#include "media/capture/video/chromeos/mojom/camera_app.mojom.h"
#include "ui/aura/window.h"
#include "ui/webui/mojo_web_ui_controller.h"

namespace chromeos_camera {
class CameraAppHelperImpl;
}  // namespace chromeos_camera

namespace media {
class CameraAppDeviceProviderImpl;
}  // namespace media

namespace chromeos {

class CameraAppUI : public ui::MojoWebUIController,
                    public content::DevToolsAgentHostObserver {
 public:
  CameraAppUI(content::WebUI* web_ui,
              std::unique_ptr<CameraAppUIDelegate> delegate);
  ~CameraAppUI() override;

  // [To be deprecated] This method is only used for CCA as a platform app and
  // will be deprecated once we migrate CCA to SWA.
  // Connects to CameraAppDeviceProvider which could be used to get
  // CameraAppDevice from video capture service through CameraAppDeviceBridge.
  static void ConnectToCameraAppDeviceProvider(
      content::RenderFrameHost* source,
      mojo::PendingReceiver<cros::mojom::CameraAppDeviceProvider> receiver);

  // [To be deprecated] This method is only used for CCA as a platform app and
  // will be deprecated once we migrate CCA to SWA.
  // Connects to CameraAppHelper that could handle camera intents.
  static void ConnectToCameraAppHelper(
      content::RenderFrameHost* source,
      mojo::PendingReceiver<chromeos_camera::mojom::CameraAppHelper> receiver);

  // Instantiates implementor of the cros::mojom::CameraAppDeviceProvider mojo
  // interface passing the pending receiver that will be internally bound.
  void BindInterface(
      mojo::PendingReceiver<cros::mojom::CameraAppDeviceProvider> receiver);

  // Instantiates implementor of the chromeos_camera::mojom::CameraAppHelper
  // mojo interface passing the pending receiver that will be internally bound.
  void BindInterface(
      mojo::PendingReceiver<chromeos_camera::mojom::CameraAppHelper> receiver);

  CameraAppUIDelegate* delegate() { return delegate_.get(); }

  aura::Window* window();

  CameraAppWindowManager* app_window_manager();

  const GURL& url();

  // content::DevToolsAgentHostObserver overrides.
  void DevToolsAgentHostAttached(
      content::DevToolsAgentHost* agent_host) override;
  void DevToolsAgentHostDetached(
      content::DevToolsAgentHost* agent_host) override;

 private:
  std::unique_ptr<CameraAppUIDelegate> delegate_;

  std::unique_ptr<media::CameraAppDeviceProviderImpl> provider_;

  std::unique_ptr<chromeos_camera::CameraAppHelperImpl> helper_;

  WEB_UI_CONTROLLER_TYPE_DECL();

  DISALLOW_COPY_AND_ASSIGN(CameraAppUI);
};

}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_CAMERA_APP_UI_CAMERA_APP_UI_H_
