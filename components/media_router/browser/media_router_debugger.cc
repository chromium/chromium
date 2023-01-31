// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/media_router/browser/media_router_debugger.h"

#include "media/base/media_switches.h"

namespace media_router {

MediaRouterDebugger::MediaRouterDebugger() = default;
MediaRouterDebugger::~MediaRouterDebugger() = default;

void MediaRouterDebugger::EnableRtcpReports() {
  is_rtcp_reports_enabled_ = true;
}

void MediaRouterDebugger::DisableRtcpReports() {
  is_rtcp_reports_enabled_ = false;
}

bool MediaRouterDebugger::IsRtcpReportsEnabled() const {
  return is_rtcp_reports_enabled_ &&
         base::FeatureList::IsEnabled(media::kEnableRtcpReporting);
}

}  // namespace media_router
