// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/base/mixin_based_in_process_browser_test.h"

// Implementation of MixinBasedInProcessBrowserTest.
#if BUILDFLAG(IS_ANDROID)
template class InProcessBrowserTestMixinHostSupport<AndroidBrowserTest>;
#else
template class InProcessBrowserTestMixinHostSupport<InProcessBrowserTest>;
#endif
