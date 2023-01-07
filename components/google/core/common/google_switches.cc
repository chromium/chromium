// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/google/core/common/google_switches.h"

namespace switches {

// Specifies an alternate URL to use for speaking to Google. Useful for testing.
const char kGoogleBaseURL[] = "google-base-url";

// When set, this will ignore the PortPermission passed in the google_util.h
// methods and ignore the port numbers. This makes it easier to run tests for
// features that use these methods (directly or indirectly) with the
// EmbeddedTestServer, which is more representative of production.
const char kIgnoreGooglePortNumbers[] = "ignore-google-port-numbers";

}  // namespace switches
