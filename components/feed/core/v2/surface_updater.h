// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FEED_CORE_V2_SURFACE_UPDATER_H_
#define COMPONENTS_FEED_CORE_V2_SURFACE_UPDATER_H_

#include <map>
#include <string>
#include <vector>

#include "base/containers/flat_set.h"
#include "base/observer_list.h"
#include "components/feed/core/proto/v2/ui.pb.h"
#include "components/feed/core/v2/enums.h"
#include "components/feed/core/v2/public/feed_stream_api.h"
#include "components/feed/core/v2/stream_model.h"

namespace feedui {
class StreamUpdate;
}  // namespace feedui
namespace feed {
class MetricsReporter;

// Keeps the UI up to date by calling |SurfaceInterface::StreamUpdate()|.
// Updates are triggered when |StreamModel| changes, or when loading state
// changes (for spinners and zero-state).
class SurfaceUpdater : public StreamModel::Observer {
 public:
  using SurfaceInterface = FeedStreamApi::SurfaceInterface;

  explicit SurfaceUpdater(MetricsReporter* metrics_reporter);
  ~SurfaceUpdater() override;
  SurfaceUpdater(const SurfaceUpdater&) = delete;
  SurfaceUpdater& operator=(const SurfaceUpdater&) = delete;

  // Sets or unsets the model. When |model| is non-null, triggers the population
  // of surfaces. When |model| is null, this does not send any updates to
  // surfaces, so they will keep any content they may have been displaying
  // before. We don't send a zero-state in this case, since we might want to
  // immedately trigger a load.
  void SetModel(StreamModel* model);

  // StreamModel::Observer.
  void OnUiUpdate(const StreamModel::UiUpdate& update) override;

  // Signals from |FeedStream|.
  void SurfaceAdded(SurfaceInterface* surface);
  void SurfaceRemoved(SurfaceInterface* surface);
  // Called to indicate the initial model load is in progress.
  void LoadStreamStarted();
  void LoadStreamComplete(bool success, LoadStreamStatus load_stream_status);
  // Called to indicate whether or not we are currently trying to load more
  // content at the bottom of the stream.
  void SetLoadingMore(bool is_loading);

  // Returns the 0-based index of the slice in the stream, or -1 if the slice is
  // not found. Ignores all non-content slices.
  int GetSliceIndexFromSliceId(const std::string& slice_id);

  // Returns whether or not at least one surface is attached.
  bool HasSurfaceAttached() const;

  void SetOfflinePageAvailability(const std::string& badge_id,
                                  bool available_offline);

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
  void SendStreamUpdateIfNeeded();
  void SendStreamUpdate(
      const std::vector<std::string>& updated_shared_state_ids);
  void SendUpdateToSurface(SurfaceInterface* surface,
                           const feedui::StreamUpdate& update);
  void InsertDatastoreEntry(const std::string& key, const std::string& value);
  void RemoveDatastoreEntry(const std::string& key);

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

  // XSurface datastore entries that should be sent to all surfaces.
  // Cached here so that we don't need to recompute for a new surface.
  std::map<std::string, std::string> xsurface_datastore_entries_;

  // Owned by |FeedStream|. Null when the model is not loaded.
  StreamModel* model_ = nullptr;
  // Owned by |FeedStream|.
  MetricsReporter* metrics_reporter_;

  // Attached surfaces.
  base::ObserverList<SurfaceInterface> surfaces_;
};
}  // namespace feed

#endif  // COMPONENTS_FEED_CORE_V2_SURFACE_UPDATER_H_
