// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_GUEST_VIEW_BROWSER_SLIM_WEB_VIEW_SLIM_WEB_VIEW_CONSTANTS_H_
#define COMPONENTS_GUEST_VIEW_BROWSER_SLIM_WEB_VIEW_SLIM_WEB_VIEW_CONSTANTS_H_

namespace guest_view::slim_web_view {

// Events.
extern const char kEventContentLoad[];
extern const char kEventLoadAbort[];
extern const char kEventLoadCommit[];
extern const char kEventNewWindow[];

// Parameters on events.
extern const char kInitialHeight[];
extern const char kInitialWidth[];
extern const char kRequestInfo[];
extern const char kTargetURL[];
extern const char kWindowOpenDisposition[];

}  // namespace guest_view::slim_web_view

#endif  // COMPONENTS_GUEST_VIEW_BROWSER_SLIM_WEB_VIEW_SLIM_WEB_VIEW_CONSTANTS_H_
