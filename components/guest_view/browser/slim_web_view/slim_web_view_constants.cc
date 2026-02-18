// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/guest_view/browser/slim_web_view/slim_web_view_constants.h"

namespace guest_view::slim_web_view {

// Events.
const char kEventContentLoad[] = "contentload";
const char kEventLoadAbort[] = "loadabort";
const char kEventLoadCommit[] = "loadcommit";
const char kEventNewWindow[] = "newwindow";

// Parameters on events.
const char kInitialHeight[] = "initialHeight";
const char kInitialWidth[] = "initialWidth";
const char kRequestInfo[] = "requestInfo";
const char kTargetURL[] = "targetUrl";
const char kWindowOpenDisposition[] = "windowOpenDisposition";

}  // namespace guest_view::slim_web_view
