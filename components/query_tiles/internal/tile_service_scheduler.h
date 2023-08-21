// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_QUERY_TILES_INTERNAL_TILE_SERVICE_SCHEDULER_H_
#define COMPONENTS_QUERY_TILES_INTERNAL_TILE_SERVICE_SCHEDULER_H_

#include "components/query_tiles/internal/tile_types.h"

namespace query_tiles {

struct TileGroup;

// Coordinates with native background task scheduler to schedule or cancel a
// TileBackgroundTask.
class TileServiceScheduler {
 public:
  class Delegate {
   public:
    Delegate() = default;
    virtual ~Delegate() = default;

    Delegate(const Delegate& other) = delete;
    Delegate& operator=(const Delegate& other) = delete;

    // Returns the tile group instance holds in memory.
    virtual TileGroup* GetTileGroup() = 0;
  };

  // Set delegate object for the scheduler.
  virtual void SetDelegate(Delegate* delegate) = 0;

  // Called when fetching task starts.
  virtual void OnFetchStarted() = 0;

  // Called on fetch task completed, schedule another task with or without
  // backoff based on the status. Success status will lead a regular schedule
  // after around 14 - 18 hours. Failure status will lead a backoff, the release
  // duration is related to count of failures. Suspend status will directly set
  // the release time until 24 hours later.
  virtual void OnFetchCompleted(TileInfoRequestStatus status) = 0;

  // Called on tile manager initialization completed, schedule another task with
  // or without backoff based on the status. NoTiles status will lead a regular
  // schedule after around 14 - 18 hours. DbOperationFailure status will
  // directly set the release time until 24 hours later.
  virtual void OnTileManagerInitialized(TileGroupStatus status) = 0;

  // Called when database is purged. Reset the flow and update the status.
  virtual void OnDbPurged(TileGroupStatus status) = 0;

  // Called when parsed group data are saved.
  virtual void OnGroupDataSaved(TileGroupStatus status) = 0;

  // Cancel current existing task, and reset scheduler.
  virtual void CancelTask() = 0;

  virtual ~TileServiceScheduler() = default;

  TileServiceScheduler(const TileServiceScheduler& other) = delete;
  TileServiceScheduler& operator=(const TileServiceScheduler& other) = delete;

 protected:
  TileServiceScheduler() = default;
};

}  // namespace query_tiles

#endif  // COMPONENTS_QUERY_TILES_INTERNAL_TILE_SERVICE_SCHEDULER_H_
