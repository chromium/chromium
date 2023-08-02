// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_REMOTE_COCOA_APP_SHIM_NS_VIEW_IDS_H_
#define COMPONENTS_REMOTE_COCOA_APP_SHIM_NS_VIEW_IDS_H_

#include <stdint.h>

#include "components/remote_cocoa/app_shim/remote_cocoa_app_shim_export.h"

@class NSView;

namespace remote_cocoa {

// Return an NSView given its id. This is to be called in an app shim process.
NSView* REMOTE_COCOA_APP_SHIM_EXPORT GetNSViewFromId(uint64_t ns_view_id);

// Return an id given an NSView. Returns 0 if |ns_view| does not have an
// associated id.
uint64_t REMOTE_COCOA_APP_SHIM_EXPORT GetIdFromNSView(NSView* ns_view);

// A scoped mapping from |ns_view_id| to |view|. While this object exists,
// GetNSViewFromId will return |view| when queried with |ns_view_id|. This
// is to be instantiated in the app shim process.
class REMOTE_COCOA_APP_SHIM_EXPORT ScopedNSViewIdMapping {
 public:
  ScopedNSViewIdMapping(uint64_t ns_view_id, NSView* view);

  ScopedNSViewIdMapping(const ScopedNSViewIdMapping&) = delete;
  ScopedNSViewIdMapping& operator=(const ScopedNSViewIdMapping&) = delete;

  ~ScopedNSViewIdMapping();

 private:
  NSView* const __strong ns_view_;
  const uint64_t ns_view_id_;
};

}  // namespace remote_cocoa

#endif  // COMPONENTS_REMOTE_COCOA_APP_SHIM_NS_VIEW_IDS_H_
