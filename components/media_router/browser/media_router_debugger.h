// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MEDIA_ROUTER_BROWSER_MEDIA_ROUTER_DEBUGGER_H_
#define COMPONENTS_MEDIA_ROUTER_BROWSER_MEDIA_ROUTER_DEBUGGER_H_

#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "base/sequence_checker.h"
#include "base/values.h"

namespace media_router {

// An interface for media router debugging and feedback.
class MediaRouterDebugger {
 public:
  virtual ~MediaRouterDebugger() = default;

  class MirroringStatsObserver : public base::CheckedObserver {
   public:
    virtual void OnMirroringStatsUpdated(
        const base::Value::Dict& json_logs) = 0;
  };

  // Gets the mirroring stats in a Dict, only for the purposes of printing to
  // logs. The Dict has no guaranteed structure.
  virtual base::Value::Dict GetMirroringStats() = 0;

  virtual void AddObserver(MirroringStatsObserver& obs) = 0;
  virtual void RemoveObserver(MirroringStatsObserver& obs) = 0;

  // Enables Rtcp fetching and analysis for future mirroring sessions.
  virtual void EnableRtcpReports() = 0;

  // Disables Rtcp fetching and analysis for future mirroring sessions.
  virtual void DisableRtcpReports() = 0;

  // Returns whether Rtcp reports are enabled.
  virtual bool ShouldFetchMirroringStats() const = 0;
};

}  // namespace media_router

#endif  // COMPONENTS_MEDIA_ROUTER_BROWSER_MEDIA_ROUTER_DEBUGGER_H_
