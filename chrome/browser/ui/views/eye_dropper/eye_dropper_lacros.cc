// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/memory/raw_ptr.h"
#include "chromeos/crosapi/mojom/eye_dropper.mojom.h"
#include "chromeos/lacros/lacros_service.h"
#include "content/public/browser/eye_dropper.h"
#include "content/public/browser/eye_dropper_listener.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "ui/base/ui_base_features.h"

namespace {

class SystemEyeDropper : public crosapi::mojom::EyeDropperListener,
                         public content::EyeDropper {
 public:
  explicit SystemEyeDropper(content::EyeDropperListener* listener)
      : listener_(listener) {
    chromeos::LacrosService::Get()
        ->GetRemote<crosapi::mojom::EyeDropper>()
        ->ShowEyeDropper(receiver.BindNewPipeAndPassRemoteWithVersion());
  }

  SystemEyeDropper(const SystemEyeDropper&) = delete;
  SystemEyeDropper& operator=(const SystemEyeDropper&) = delete;

  ~SystemEyeDropper() override = default;

 private:
  void ColorSelected(SkColor color) override {
    listener_->ColorSelected(color);
  }

  void ColorSelectionCanceled() override {
    listener_->ColorSelectionCanceled();
  }

  raw_ptr<content::EyeDropperListener> listener_;
  mojo::Receiver<crosapi::mojom::EyeDropperListener> receiver{this};
};

}  // namespace

std::unique_ptr<content::EyeDropper> ShowEyeDropper(
    content::RenderFrameHost* frame,
    content::EyeDropperListener* listener) {
  if (!features::IsEyeDropperEnabled() || !frame->GetView()->HasFocus() ||
      !chromeos::LacrosService::Get()
           ->IsSupported<crosapi::mojom::EyeDropper>()) {
    return nullptr;
  }
  return std::make_unique<SystemEyeDropper>(listener);
}
