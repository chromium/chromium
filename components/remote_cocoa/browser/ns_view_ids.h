// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_REMOTE_COCOA_BROWSER_NS_VIEW_IDS_H_
#define COMPONENTS_REMOTE_COCOA_BROWSER_NS_VIEW_IDS_H_

#include <stdint.h>

#include "components/remote_cocoa/browser/remote_cocoa_browser_export.h"

namespace remote_cocoa {

// Return a new unique is to be used with ScopedNSViewIdMapping and
// GetNSViewFromId in various app shim processes.
uint64_t REMOTE_COCOA_BROWSER_EXPORT GetNewNSViewId();

}  // namespace remote_cocoa

#endif  // COMPONENTS_REMOTE_COCOA_BROWSER_NS_VIEW_IDS_H_
