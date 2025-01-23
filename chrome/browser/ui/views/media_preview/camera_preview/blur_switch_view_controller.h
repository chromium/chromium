// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_MEDIA_PREVIEW_CAMERA_PREVIEW_BLUR_SWITCH_VIEW_CONTROLLER_H_
#define CHROME_BROWSER_UI_VIEWS_MEDIA_PREVIEW_CAMERA_PREVIEW_BLUR_SWITCH_VIEW_CONTROLLER_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "content/public/browser/browser_context.h"
#include "media/capture/mojom/video_effects_manager.mojom.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace views {
class ToggleButton;
class View;
}  // namespace views

// Controller for the blur switch. This is only intended to be used for
// debugging purposes.
class BlurSwitchViewController
    : public media::mojom::VideoEffectsConfigurationObserver {
 public:
  explicit BlurSwitchViewController(
      views::View& parent_view,
      base::WeakPtr<content::BrowserContext> browser_context);
  BlurSwitchViewController(const BlurSwitchViewController&) = delete;
  BlurSwitchViewController& operator=(const BlurSwitchViewController&) = delete;
  ~BlurSwitchViewController() override;

  void BindVideoEffectsManager(const std::string& active_device_id);
  void ResetConnections();

 private:
  void OnBlurSwitchPressed();

  // media::mojom::VideoEffectsConfigurationObserver impl.
  void OnConfigurationChanged(
      media::mojom::VideoEffectsConfigurationPtr configuration) override;

  base::WeakPtr<content::BrowserContext> browser_context_;
  raw_ptr<views::ToggleButton> blur_switch_;

  mojo::Remote<media::mojom::VideoEffectsManager> video_effects_manager_;
  mojo::Receiver<media::mojom::VideoEffectsConfigurationObserver>
      video_effects_configuration_observer_{this};

  base::WeakPtrFactory<BlurSwitchViewController> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_MEDIA_PREVIEW_CAMERA_PREVIEW_BLUR_SWITCH_VIEW_CONTROLLER_H_
