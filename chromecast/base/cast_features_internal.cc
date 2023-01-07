// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/feature_list.h"

namespace chromecast {
std::vector<const base::Feature*> GetInternalFeatures() {
  return std::vector<const base::Feature*>();
}
}  // namespace chromecast
