// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_DISPLAY_SOFTWARE_OUTPUT_DEVICE_CLIENT_H_
#define COMPONENTS_VIZ_SERVICE_DISPLAY_SOFTWARE_OUTPUT_DEVICE_CLIENT_H_

namespace gfx {
struct CALayerParams;
}  // namespace gfx

namespace viz {

class SoftwareOutputDeviceClient {
 public:
  virtual ~SoftwareOutputDeviceClient() = default;

  // Specify the CALayer parameters used to display the content drawn by this
  // device on macOS.
  virtual void SoftwareDeviceUpdatedCALayerParams(
      const gfx::CALayerParams& ca_layer_params) = 0;
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_DISPLAY_SOFTWARE_OUTPUT_DEVICE_CLIENT_H_
