// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/test/constants.h"

namespace vr {

gfx::Transform GetPixelDaydreamProjMatrix() {
  return gfx::Transform::RowMajor(1.03317f, 0.0f, 0.271253f, 0.0f, 0.0f,
                                  0.862458f, -0.0314586f, 0.0f, 0.0f, 0.0f,
                                  -1.002f, -0.2002f, 0.0f, 0.0f, -1.0f, 0.0f);
}

}  // namespace vr
