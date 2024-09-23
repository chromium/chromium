// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/public/base/wait_for_network_callback_helper.h"

#include "base/notreached.h"

void WaitForNetworkCallbackHelper::DisableNetworkCallsDelayedForTesting(
    bool disable) {
  // Subclasses that want to use this function for testing should override this
  // method.
  NOTREACHED();
}
