// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_TEST_CONSTANTS_H_
#define CHROME_BROWSER_VR_TEST_CONSTANTS_H_

#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/transform.h"
#include "ui/gfx/geometry/vector3d_f.h"

namespace vr {

// Proj matrix as used on a Pixel phone with the Daydream headset.
gfx::Transform GetPixelDaydreamProjMatrix();
static constexpr gfx::Transform kStartHeadPose;
static constexpr gfx::Vector3dF kStartControllerPosition(0.3, -0.3, -0.3);
static constexpr gfx::Vector3dF kForwardVector(0.0f, 0.0f, -1.0f);
static constexpr gfx::Vector3dF kBackwardVector(0.0f, 0.0f, 1.0f);

static constexpr float kEpsilon = 1e-5f;

// Resolution of Pixel Phone for one eye.
static constexpr gfx::Size kPixelHalfScreen(960, 1080);

static constexpr const char* kLoremIpsum100Chars =
    "Lorem ipsum dolor sit amet, consectetur adipiscing elit. Duis erat nisl, "
    "tempus nec neque at nullam.";
static constexpr const char* kLoremIpsum700Chars =
    "Lorem ipsum dolor sit amet, consectetuer adipiscing elit. Aenean commodo "
    "ligula eget dolor. Aenean massa. Cum sociis natoque penatibus et magnis "
    "dis parturient montes, nascetur ridiculus mus. Donec quam felis, "
    "ultricies nec, pellentesque eu, pretium quis, sem. Nulla consequat massa "
    "quis enim. Donec pede justo, fringilla vel, aliquet nec, vulputate eget, "
    "arcu. In enim justo, rhoncus ut, imperdiet a, venenatis vitae, justo. "
    "Nullam dictum felis eu pede mollis pretium. Integer tincidunt. Cras "
    "dapibus. Vivamus elementum semper nisi. Aenean vulputate eleifend tellus. "
    "Aenean leo ligula, porttitor eu, consequat vitae, eleifend ac, enim. "
    "Aliquam lorem ante, dapibus in, viverra quis, feugiat a, tellus";

}  // namespace vr

#endif  // CHROME_BROWSER_VR_TEST_CONSTANTS_H_
