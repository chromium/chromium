// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MEDIA_ROUTER_BROWSER_MEDIA_ROUTER_DEBUGGER_H_
#define COMPONENTS_MEDIA_ROUTER_BROWSER_MEDIA_ROUTER_DEBUGGER_H_

namespace media_router {

// An interface for media router debugging and feedback.
class MediaRouterDebugger {
 public:
  MediaRouterDebugger();

  MediaRouterDebugger(const MediaRouterDebugger&) = delete;
  MediaRouterDebugger& operator=(const MediaRouterDebugger&) = delete;

  virtual ~MediaRouterDebugger();

  void EnableRtcpReports();
  void DisableRtcpReports();

  bool IsRtcpReportsEnabled() const;

 protected:
  bool is_rtcp_reports_enabled_ = false;
};

}  // namespace media_router

#endif  // COMPONENTS_MEDIA_ROUTER_BROWSER_MEDIA_ROUTER_DEBUGGER_H_
