// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_RENDERER_SETTINGS_UI_BINDINGS_H_
#define CHROMECAST_RENDERER_SETTINGS_UI_BINDINGS_H_

#include <string>

#include "base/memory/weak_ptr.h"
#include "base/timer/timer.h"
#include "chromecast/common/mojom/settings_ui.mojom.h"
#include "chromecast/renderer/native_bindings_helper.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "v8/include/v8.h"

namespace chromecast {
namespace shell {

// Exposed to the platform settings UI app on display assistants. Handles
// certain swipe events to show/hide the settings menu. This also receives
// platform information periodically to update the device properties in the
// settings UI.
class SettingsUiBindings : public CastBinding,
                           public chromecast::mojom::SettingsClient {
 public:
  explicit SettingsUiBindings(content::RenderFrame* frame);
  ~SettingsUiBindings() override;
  SettingsUiBindings(const SettingsUiBindings&) = delete;
  SettingsUiBindings& operator=(const SettingsUiBindings&) = delete;

 private:
  friend class ::chromecast::CastBinding;

  // mojom::SettingsClient implementation:
  void HandleSideSwipe(chromecast::mojom::SideSwipeEvent event,
                       chromecast::mojom::SideSwipeOrigin origin,
                       const gfx::Point& touch_location) override;
  void SendPlatformInfo(const std::string& platform_info_json) override;

  // CastBinding implementation:
  void Install(v8::Local<v8::Object> cast_platform,
               v8::Isolate* isolate) override;

  // Binding methods
  void SetSideSwipeHandler(v8::Local<v8::Function> side_swipe_handler);
  void SetPlatformInfoHandler(v8::Local<v8::Function> platform_info_handler);
  void RequestVisible(bool visible);

  void ReconnectMojo();
  void OnMojoConnectionError();

  std::string pending_platform_info_json_;

  v8::UniquePersistent<v8::Function> side_swipe_handler_;
  v8::UniquePersistent<v8::Function> platform_info_handler_;

  mojo::Remote<chromecast::mojom::SettingsPlatform> settings_platform_ptr_;
  mojo::Receiver<chromecast::mojom::SettingsClient> binding_;
  base::RepeatingTimer mojo_reconnect_timer_;

  base::WeakPtrFactory<SettingsUiBindings> weak_factory_;
};

}  // namespace shell
}  // namespace chromecast

#endif  // CHROMECAST_RENDERER_SETTINGS_UI_BINDINGS_H_
