// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/model/model.h"

#include "base/containers/adapters.h"
#include "base/notreached.h"

namespace vr {

Model::Model() = default;
Model::~Model() = default;

const ColorScheme& Model::color_scheme() const {
  return ColorScheme::GetColorScheme();
}

}  // namespace vr
