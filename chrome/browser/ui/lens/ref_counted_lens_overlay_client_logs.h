// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_LENS_REF_COUNTED_LENS_OVERLAY_CLIENT_LOGS_H_
#define CHROME_BROWSER_UI_LENS_REF_COUNTED_LENS_OVERLAY_CLIENT_LOGS_H_

#include "third_party/lens_server_proto/lens_overlay_client_logs.pb.h"

namespace lens {

// Wrapper for LensOverlayClientLogs proto to be RefCounted so the proto can be
// passed across threads.
class RefCountedLensOverlayClientLogs
    : public base::RefCountedThreadSafe<RefCountedLensOverlayClientLogs> {
 public:
  RefCountedLensOverlayClientLogs() = default;

  lens::LensOverlayClientLogs& client_logs() { return client_logs_; }

 private:
  friend class base::RefCountedThreadSafe<RefCountedLensOverlayClientLogs>;
  ~RefCountedLensOverlayClientLogs() = default;

  lens::LensOverlayClientLogs client_logs_;
};

}  // namespace lens

#endif  // CHROME_BROWSER_UI_LENS_REF_COUNTED_LENS_OVERLAY_CLIENT_LOGS_H_
