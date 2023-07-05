// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FEED_CORE_V2_SURFACE_UPDATER_H_
#define COMPONENTS_FEED_CORE_V2_SURFACE_UPDATER_H_

#include <deque>
#include <map>
#include <string>
#include <vector>

#include "base/containers/flat_set.h"
#include "base/memory/raw_ptr.h"
#include "components/feed/core/proto/v2/ui.pb.h"
#include "components/feed/core/proto/v2/wire/reliability_logging_enums.pb.h"
#include "components/feed/core/v2/enums.h"
#include "components/feed/core/v2/launch_reliability_logger.h"
#include "components/feed/core/v2/stream_model.h"
#include "components/feed/core/v2/stream_surface_set.h"
#include "components/feed/core/v2/types.h"
#include "components/feed/core/v2/xsurface_datastore.h"

namespace feedui {
class StreamUpdate;
}  // namespace feedui
namespace feed {
class SurfaceRenderer;
class MetricsReporter;

// Keeps the UI up to date by calling |FeedStreamSurface::StreamUpdate()|.
// Updates are triggered when |StreamModel| changes, or when loading state
// changes (for spinners and zero-state).
class SurfaceUpdater : public StreamModel::Observer,
                       public StreamSurfaceSet::Observer,
                       public XsurfaceDatastoreDataReader::Observer {
 public:
  explicit SurfaceUpdater(MetricsReporter* metrics_reporter,
                          XsurfaceDatastoreDataReader* global_datastore_slice,
                          StreamSurfaceSet* surfaces);
  ~SurfaceUpdater() override;
  SurfaceUpdater(const SurfaceUpdater&) = delete;
  SurfaceUpdater& operator=(const SurfaceUpdater&) = delete;

  // Sets or unsets the model. When |model| is non-null, triggers the population
  // of surfaces. When |model| is null, this does not send any updates to
  // surfaces, so they will keep any content they may have been displaying
  // before. We don't send a zero-state in this case, since we might want to
  // immediately trigger a load.
  void SetModel(StreamModel* model);

  // StreamModel::Observer.
  void OnUiUpdate(const StreamModel::UiUpdate& update) override;

  // StreamSurfaceSet::Observer.
  void SurfaceAdded(
      SurfaceId surface_id,
      SurfaceRenderer* renderer,
      feedwire::DiscoverLaunchResult loading_not_allowed_reason) override;
  void SurfaceRemoved(SurfaceId surface_id) override;

  // XsurfaceDatastoreDataReader::Observer.
  void DatastoreEntryUpdated(XsurfaceDatastoreDataReader* source,
                             const std::string& key) override;
  void DatastoreEntryRemoved(XsurfaceDatastoreDataReader* source,
                             const std::string& key) override;

  // Called to indicate the initial model load is in progress.
  void LoadStreamStarted(bool manual_refreshing);
  void LoadStreamComplete(bool success,
                          LoadStreamStatus load_stream_status,
                          feedwire::DiscoverLaunchResult launch_result);

  // Called to indicate whether or not we are currently trying to load more
  // content at the bottom of the stream.
  void SetLoadingMore(bool is_loading);

  // Returns the 0-based index of the slice in the stream, or -1 if the slice is
  // not found. Ignores all non-content slices.
  int GetSliceIndexFromSliceId(const std::string& slice_id);

  void SetOfflinePageAvailability(const std::string& badge_id,
                                  bool available_offline);

  LaunchReliabilityLogger& launch_reliability_logger() {
    return launch_reliability_logger_;
  }

  // State that together with |model_| determines what should be sent to a
  // surface. |DrawState| is usually the same for all surfaces, except for the
  // moment when a surface is first attached.
  struct DrawState {
    bool loading_more = false;
    bool loading_initial = false;
    feedui::ZeroStateSlice::Type zero_state_type =
        feedui::ZeroStateSlice::UNKNOWN;

    bool operator==(const DrawState& rhs) const;
  };

 private:
  DrawState GetState() const;
  bool ShouldSendStreamUpdate() const;
  void SendStreamUpdateIfNeeded();
  void SendStreamUpdate(
      const std::vector<std::string>& updated_shared_state_ids);
  void SendUpdateToSurface(SurfaceId surface_id,
                           SurfaceRenderer* surface,
                           const feedui::StreamUpdate& update);
  void InsertDatastoreEntry(const std::string& key, const std::string& value);
  void RemoveDatastoreEntry(const std::string& key);

  // Owned by |FeedStream|.
  raw_ptr<MetricsReporter> metrics_reporter_;
  raw_ptr<StreamSurfaceSet> surfaces_;
  // Per-StreamType xsurface data.
  XsurfaceDatastoreSlice surface_data_slice_;
  // Combines `surface_data_slice_`, and the global data.
  XsurfaceDatastoreAggregate aggregate_data_;

  // Members that affect what is sent to surfaces. A value change of these may
  // require sending an update to surfaces.
  bool loading_more_ = false;
  bool loading_initial_ = false;
  bool load_stream_failed_ = false;
  LoadStreamStatus load_stream_status_ = LoadStreamStatus::kNoStatus;

  // The |DrawState| when the last update was sent to all surfaces.
  DrawState last_draw_state_;

  // The set of content that has been sent to all attached surfaces.
  base::flat_set<ContentRevision> sent_content_;

  // Owned by |FeedStream|. Null when the model is not loaded.
  raw_ptr<StreamModel, DanglingUntriaged> model_ = nullptr;

  LaunchReliabilityLogger launch_reliability_logger_;
  bool load_stream_started_ = false;

  int current_load_more_indicator_id_ = 0;
};
}  // namespace feed

#endif  // COMPONENTS_FEED_CORE_V2_SURFACE_UPDATER_H_
