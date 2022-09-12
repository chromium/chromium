// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_EXTERNAL_MOJO_EXTERNAL_SERVICE_SUPPORT_PROCESS_SETUP_H_
#define CHROMECAST_EXTERNAL_MOJO_EXTERNAL_SERVICE_SUPPORT_PROCESS_SETUP_H_

namespace chromecast {
namespace external_service_support {

// Common initialization code for external Mojo processes. Should be called
// early in main().
void CommonProcessInitialization(int argc, const char* const* argv);

}  // namespace external_service_support
}  // namespace chromecast

#endif  // CHROMECAST_EXTERNAL_MOJO_EXTERNAL_SERVICE_SUPPORT_PROCESS_SETUP_H_
