// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_GUEST_VIEW_BROWSER_SLIM_WEB_VIEW_SLIM_WEB_VIEW_CONSTANTS_H_
#define COMPONENTS_GUEST_VIEW_BROWSER_SLIM_WEB_VIEW_SLIM_WEB_VIEW_CONSTANTS_H_

namespace guest_view::slim_web_view {

// Events.
extern const char kEventContentLoad[];
extern const char kEventExit[];
extern const char kEventLoadAbort[];
extern const char kEventLoadCommit[];
extern const char kEventLoadStart[];
extern const char kEventLoadStop[];
extern const char kEventNewWindow[];
extern const char kEventPermission[];
extern const char kEventSizeChanged[];
extern const char kEventUnresponsive[];

// Parameters on events.
extern const char kInitialHeight[];
extern const char kInitialWidth[];
extern const char kNewHeight[];
extern const char kNewWidth[];
extern const char kOldHeight[];
extern const char kOldWidth[];
extern const char kPermission[];
extern const char kProcessId[];
extern const char kReason[];
extern const char kRequestId[];
extern const char kRequestInfo[];
extern const char kTargetURL[];
extern const char kWindowOpenDisposition[];

}  // namespace guest_view::slim_web_view

#endif  // COMPONENTS_GUEST_VIEW_BROWSER_SLIM_WEB_VIEW_SLIM_WEB_VIEW_CONSTANTS_H_
