// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"
#include "components/os_crypt/sync/os_crypt_switches.h"

namespace os_crypt {
namespace switches {

#if BUILDFLAG(IS_APPLE)

const char kUseMockKeychain[] = "use-mock-keychain";

#endif  // BUILDFLAG(IS_APPLE)

}  // namespace switches
}  // namespace os_crypt
