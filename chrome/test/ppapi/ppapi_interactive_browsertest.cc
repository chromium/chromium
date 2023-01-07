// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/ppapi/ppapi_test.h"

#include "build/build_config.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "content/public/test/browser_test.h"
#include "ppapi/shared_impl/test_utils.h"

// Disable tests under ASAN.  http://crbug.com/104832.
// This is a bit heavy handed, but the majority of these tests fail under ASAN.
// See bug for history.
// Flaky on Win/Mac, http://crbug.com/1048148.
#if defined(ADDRESS_SANITIZER) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || \
    BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
#define MAYBE_MouseLock_SucceedWhenAllowed DISABLED_MouseLock_SucceedWhenAllowed
#else
#define MAYBE_MouseLock_SucceedWhenAllowed MouseLock_SucceedWhenAllowed
#endif  // ADDRESS_SANITIZER
IN_PROC_BROWSER_TEST_F(OutOfProcessPPAPITest,
                       MAYBE_MouseLock_SucceedWhenAllowed) {
  RunTestViaHTTP("MouseLock_SucceedWhenAllowed");
}

IN_PROC_BROWSER_TEST_F(OutOfProcessPPAPITest, DISABLED_ImeInputEvent) {
  RunTest(ppapi::StripTestPrefixes("ImeInputEvent"));
}
