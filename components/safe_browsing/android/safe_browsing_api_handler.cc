// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/android/safe_browsing_api_handler.h"

#include "base/no_destructor.h"
#include "components/safe_browsing/android/safe_browsing_api_handler_bridge.h"
#include "components/safe_browsing/buildflags.h"

namespace safe_browsing {

// static
SafeBrowsingApiHandler* SafeBrowsingApiHandler::GetInstance() {
#if BUILDFLAG(SAFE_BROWSING_DB_REMOTE)
  static base::NoDestructor<SafeBrowsingApiHandlerBridge> instance;
  return instance.get();
#else
  return nullptr;
#endif
}

}  // namespace safe_browsing
