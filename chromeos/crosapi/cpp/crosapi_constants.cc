// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/crosapi/cpp/crosapi_constants.h"

namespace crosapi {

// The default user-data-directory for Lacros.
// NOTE: This is security sensitive. The directory must be inside the encrypted
// user data partition.
const char kLacrosUserDataPath[] = "/home/chronos/user/lacros";

}  // namespace crosapi
