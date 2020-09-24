// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/crosapi/cpp/crosapi_constants.h"

namespace crosapi {

// The prefix for a Wayland app id for a Lacros browser window. The full ID is
// suffixed with a serialized unguessable token unique to each window. The
// trailing "." is intentional.
const char kLacrosAppIdPrefix[] = "org.chromium.lacros.";

}  // namespace crosapi
