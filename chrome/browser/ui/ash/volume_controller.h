// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_VOLUME_CONTROLLER_H_
#define CHROME_BROWSER_UI_ASH_VOLUME_CONTROLLER_H_

#include "ash/public/interfaces/volume.mojom.h"
#include "base/macros.h"
#include "mojo/public/cpp/bindings/binding.h"

// Controls the volume when F8-10 or a multimedia key for volume is pressed.
// TODO(crbug.com/647781): Media accelerators like F8-F10 are broken in mash.
// Fix this when mash supports event rewriting.
class VolumeController : public ash::mojom::VolumeController {
 public:
  VolumeController();
  ~VolumeController() override;

  // Overridden from ash::mojom::VolumeClient:
  void VolumeMuteToggle() override;
  void VolumeDown() override;
  void VolumeUp() override;

 private:
  mojo::Binding<ash::mojom::VolumeController> binding_;

  DISALLOW_COPY_AND_ASSIGN(VolumeController);
};

#endif  // CHROME_BROWSER_UI_ASH_VOLUME_CONTROLLER_H_
