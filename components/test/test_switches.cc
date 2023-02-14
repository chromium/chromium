// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/test/test_switches.h"

namespace switches {

// Used by some tests to force Mojo broker initialization in a spawned child
// process.
extern const char kInitializeMojoAsBroker[] = "initialize-mojo-as-broker";

}  // namespace switches
