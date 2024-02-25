// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feed/core/v2/surface_updater.h"

#include <tuple>
#include <utility>

#include "base/check.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/time.h"
#include "components/feed/core/proto/v2/ui.pb.h"
#include "components/feed/core/proto/v2/wire/reliability_logging_enums.pb.h"
#include "components/feed/core/proto/v2/xsurface.pb.h"
#include "components/feed/core/v2/enums.h"
#include "components/feed/core/v2/feed_stream.h"
#include "components/feed/core/v2/feed_stream_surface.h"
#include "components/feed/core/v2/launch_reliability_logger.h"
#include "components/feed/core/v2/metrics_reporter.h"
#include "components/feed/core/v2/stream_surface_set.h"
#include "components/feed/core/v2/types.h"

namespace feed {
namespace {

using DrawState = SurfaceUpdater::DrawState;
using StreamUpdateType = LaunchReliabilityLogger::StreamUpdateType;

// Give each kind of zero state a unique name, so that the UI knows if it
// changes.
const char* GetZeroStateSliceId(feedui::ZeroStateSlice::Type type) {
  switch (type) {
    case feedui::ZeroStateSlice::NO_CARDS_AVAILABLE:
      return "no-cards";
    case feedui::ZeroStateSlice::NO_WEB_FEED_SUBSCRIPTIONS:
      return "no-subscriptions";
    case feedui::ZeroStateSlice::CANT_REFRESH:  // fall-through
    default:
      return "cant-refresh";
  }
}

void AddSharedState(const StreamModel& model,
                    const std::string& shared_state_id,
                    feedui::StreamUpdate* stream_update) {
  const std::string* shared_state_data =
      model.FindSharedStateData(shared_state_id);
  DCHECK(shared_state_data);
  feedui::SharedState* added_shared_state =
      stream_update->add_new_shared_states();
  added_shared_state->set_id(shared_state_id);
  added_shared_state->set_xsurface_shared_state(*shared_state_data);
}

void AddSliceUpdate(const StreamModel& model,
                    ContentRevision content_revision,
                    bool is_content_new,
                    feedui::StreamUpdate* stream_update) {
  if (is_content_new) {
    feedui::Slice* slice = stream_update->add_updated_slices()->mutable_slice();
    slice->set_slice_id(ToString(content_revision));
    const feedstore::Content* content = model.FindContent(content_revision);
    DCHECK(content);
    slice->mutable_xsurface_slice()->set_xsurface_frame(content->frame());
  } else {
    stream_update->add_updated_slices()->set_slice_id(
        ToString(content_revision));
  }
}

void AddLoadingSpinner(bool is_initial_load,
                       int load_more_indicator_id,
                       feedui::StreamUpdate* update) {
  feedui::Slice* slice = update->add_updated_slices()->mutable_slice();
  slice->mutable_loading_spinner_slice()->set_is_at_top(is_initial_load);
  // The slice ID is checked on Java code to find out the loading spinner view.
  // An incremental ID is added to the slice ID in order to differentiate from
  // pvrevious spinners.
  slice->set_slice_id(
      is_initial_load
          ? "loading-spinner"
          : base::StrCat({"load-more-spinner",
                          base::NumberToString(load_more_indicator_id)}));
}

struct StreamUpdateAndType {
  feedui::StreamUpdate stream_update;
  StreamUpdateType type = StreamUpdateType::kNone;
};

StreamUpdateAndType MakeStreamUpdate(
    const std::vector<std::string>& updated_shared_state_ids,
    const base::flat_set<ContentRevision>& already_sent_content,
    const StreamModel* model,
    const DrawState& state,
    int load_more_indicator_id) {
  DCHECK(!state.loading_initial || !state.loading_more)
      << "logic bug: requested both top and bottom spinners.";

  StreamUpdateAndType update;

  // Add content from the model, if it's loaded.
  bool has_content = false;
  if (model) {
    for (ContentRevision content_revision : model->GetContentList()) {
      const bool is_updated = already_sent_content.count(content_revision) == 0;
      AddSliceUpdate(*model, content_revision, is_updated,
                     &update.stream_update);
      has_content = true;
      update.type = StreamUpdateType::kContent;
    }
    for (const std::string& name : updated_shared_state_ids) {
      AddSharedState(*model, name, &update.stream_update);
    }
  }

  feedui::ZeroStateSlice::Type zero_state_type = state.zero_state_type;
  // If there are no cards, and we aren't loading, force a zero-state.
  // This happens when a model is loaded, but it has no content.
  if (!state.loading_initial && !has_content &&
      state.zero_state_type == feedui::ZeroStateSlice::UNKNOWN) {
    zero_state_type = feedui::ZeroStateSlice::NO_CARDS_AVAILABLE;
  }

  if (zero_state_type != feedui::ZeroStateSlice::UNKNOWN) {
    feedui::Slice* slice =
        update.stream_update.add_updated_slices()->mutable_slice();
    slice->mutable_zero_state_slice()->set_type(zero_state_type);
    slice->set_slice_id(GetZeroStateSliceId(zero_state_type));
    update.type = StreamUpdateType::kZeroState;
  } else {
    // Add the initial-load spinner if applicable.
    if (state.loading_initial) {
      AddLoadingSpinner(/*is_initial_load=*/true, load_more_indicator_id,
                        &update.stream_update);
      update.type = StreamUpdateType::kInitialLoadingSpinner;
    }
    // Add a loading-more spinner if applicable.
    if (state.loading_more) {
      AddLoadingSpinner(/*is_initial_load=*/false, load_more_indicator_id,
                        &update.stream_update);
      update.type = StreamUpdateType::kLoadingMoreSpinner;
    }
  }

  if (model) {
    update.stream_update.set_fetch_time_ms(
        model->GetLastAddedTime().ToDeltaSinceWindowsEpoch().InMilliseconds());
    ToProto(model->GetLoggingParameters(),
            *update.stream_update.mutable_logging_parameters());
  } else {
    ToProto(LoggingParameters{},
            *update.stream_update.mutable_logging_parameters());
  }

  return update;
}

StreamUpdateAndType GetUpdateForNewSurface(const DrawState& state,
                                           const StreamModel* model) {
  std::vector<std::string> updated_shared_state_ids;
  if (model) {
    updated_shared_state_ids = model->GetSharedStateIds();
  }
  return MakeStreamUpdate(std::move(updated_shared_state_ids),
                          /*already_sent_content=*/{}, model, state, 0);
}

base::flat_set<ContentRevision> GetContentSet(const StreamModel* model) {
  if (!model)
    return {};
  const std::vector<ContentRevision>& content_list = model->GetContentList();
  return base::flat_set<ContentRevision>(content_list.begin(),
                                         content_list.end());
}

feedui::ZeroStateSlice::Type GetZeroStateType(LoadStreamStatus status) {
  switch (status) {
    case LoadStreamStatus::kNoResponseBody:
    case LoadStreamStatus::kProtoTranslationFailed:
    case LoadStreamStatus::kCannotLoadFromNetworkOffline:
    case LoadStreamStatus::kCannotLoadFromNetworkThrottled:
    case LoadStreamStatus::kNetworkFetchFailed:
    case LoadStreamStatus::kAccountTokenFetchFailedWrongAccount:
    case LoadStreamStatus::kAccountTokenFetchTimedOut:
    case LoadStreamStatus::kNetworkFetchTimedOut:
      return feedui::ZeroStateSlice::CANT_REFRESH;
    case LoadStreamStatus::kNotAWebFeedSubscriber:
      return feedui::ZeroStateSlice::NO_WEB_FEED_SUBSCRIPTIONS;
    case LoadStreamStatus::kNoStatus:
    case LoadStreamStatus::kLoadedFromStore:
    case LoadStreamStatus::kLoadedFromNetwork:
    case LoadStreamStatus::kFailedWithStoreError:
    case LoadStreamStatus::kNoStreamDataInStore:
    case LoadStreamStatus::kModelAlreadyLoaded:
    case LoadStreamStatus::kDataInStoreIsStale:
    case LoadStreamStatus::kDataInStoreIsStaleTimestampInFuture:
    case LoadStreamStatus::
        kCannotLoadFromNetworkSupressedForHistoryDelete_DEPRECATED:
    case LoadStreamStatus::kLoadNotAllowedEulaNotAccepted:
    case LoadStreamStatus::kLoadNotAllowedArticlesListHidden:
    case LoadStreamStatus::kCannotParseNetworkResponseBody:
    case LoadStreamStatus::kLoadMoreModelIsNotLoaded:
    case LoadStreamStatus::kLoadNotAllowedDisabledByEnterprisePolicy:
    case LoadStreamStatus::kCannotLoadMoreNoNextPageToken:
    case LoadStreamStatus::kDataInStoreStaleMissedLastRefresh:
    case LoadStreamStatus::kLoadedStaleDataFromStoreDueToNetworkFailure:
    case LoadStreamStatus::kDataInStoreIsExpired:
    case LoadStreamStatus::kDataInStoreIsForAnotherUser:
    case LoadStreamStatus::kAbortWithPendingClearAll:
    case LoadStreamStatus::kAlreadyHaveUnreadContent:
    case LoadStreamStatus::kLoadNotAllowedDisabled:
    case LoadStreamStatus::kLoadNotAllowedDisabledByDse:
      break;
  }
  return feedui::ZeroStateSlice::NO_CARDS_AVAILABLE;
}

}  // namespace

bool SurfaceUpdater::DrawState::operator==(const DrawState& rhs) const {
  return std::tie(loading_more, loading_initial, zero_state_type) ==
         std::tie(rhs.loading_more, rhs.loading_initial, rhs.zero_state_type);
}

SurfaceUpdater::SurfaceUpdater(
    MetricsReporter* metrics_reporter,
    XsurfaceDatastoreDataReader* global_datastore_slice,
    StreamSurfaceSet* surfaces)
    : metrics_reporter_(metrics_reporter),
      surfaces_(surfaces),
      aggregate_data_({&surface_data_slice_, global_datastore_slice}),
      launch_reliability_logger_(surfaces) {
  aggregate_data_.AddObserver(this);
}

SurfaceUpdater::~SurfaceUpdater() {
  aggregate_data_.RemoveObserver(this);
}

void SurfaceUpdater::SetModel(StreamModel* model) {
  if (model_ == model)
    return;
  if (model_)
    model_->RemoveObserver(this);
  model_ = model;
  sent_content_.clear();
  if (model_) {
    model_->AddObserver(this);
    loading_initial_ = loading_initial_ && model_->GetContentList().empty();
    loading_more_ = false;
    // TODO(iwells): Avoid sending a second loading spinner in the "valid
    // response, zero cards" case.
    SendStreamUpdate(model_->GetSharedStateIds());
    last_draw_state_ = GetState();
  }
}

void SurfaceUpdater::OnUiUpdate(const StreamModel::UiUpdate& update) {
  DCHECK(model_);  // The update comes from the model.
  loading_initial_ = loading_initial_ && model_->GetContentList().empty();
  loading_more_ = loading_more_ && !update.content_list_changed;

  std::vector<std::string> updated_shared_state_ids;
  for (const StreamModel::UiUpdate::SharedStateInfo& info :
       update.shared_states) {
    if (info.updated)
      updated_shared_state_ids.push_back(info.shared_state_id);
  }

  SendStreamUpdate(updated_shared_state_ids);
}

void SurfaceUpdater::SurfaceAdded(
    SurfaceId surface_id,
    SurfaceRenderer* renderer,
    feedwire::DiscoverLaunchResult loading_not_allowed_reason) {
  ReliabilityLoggingBridge& logger = renderer->GetReliabilityLoggingBridge();
  logger.LogFeedLaunchOtherStart(base::TimeTicks::Now());

  if (loading_not_allowed_reason !=
      feedwire::DiscoverLaunchResult::CARDS_UNSPECIFIED) {
    logger.LogLaunchFinishedAfterStreamUpdate(loading_not_allowed_reason);
  }

  StreamUpdateAndType update = GetUpdateForNewSurface(GetState(), model_);
  launch_reliability_logger_.OnStreamUpdate(update.type, *renderer);
  SendUpdateToSurface(surface_id, renderer, update.stream_update);

  for (std::pair<std::string, std::string> datastore_entry :
       aggregate_data_.GetAllEntries()) {
    renderer->ReplaceDataStoreEntry(datastore_entry.first,
                                    datastore_entry.second);
  }
}

void SurfaceUpdater::SurfaceRemoved(SurfaceId surface_id) {}

void SurfaceUpdater::DatastoreEntryUpdated(XsurfaceDatastoreDataReader*,
                                           const std::string& key) {
  const std::string* value = aggregate_data_.FindEntry(key);
  DCHECK(value);
  for (auto& entry : *surfaces_)
    entry.renderer->ReplaceDataStoreEntry(key, *value);
}

void SurfaceUpdater::DatastoreEntryRemoved(XsurfaceDatastoreDataReader*,
                                           const std::string& key) {
  for (auto& entry : *surfaces_)
    entry.renderer->RemoveDataStoreEntry(key);
}

void SurfaceUpdater::LoadStreamStarted(bool manual_refreshing) {
  load_stream_failed_ = false;
  loading_initial_ = !manual_refreshing;
  load_stream_started_ = true;
  SendStreamUpdateIfNeeded();
}

void SurfaceUpdater::LoadStreamComplete(
    bool success,
    LoadStreamStatus load_stream_status,
    feedwire::DiscoverLaunchResult launch_result) {
  loading_initial_ = false;
  load_stream_status_ = load_stream_status;
  load_stream_failed_ = !success;

  if (ShouldSendStreamUpdate()) {
    if (launch_result != feedwire::DiscoverLaunchResult::CARDS_UNSPECIFIED) {
      launch_reliability_logger_.LogLaunchFinishedAfterStreamUpdate(
          launch_result);
    }
    SendStreamUpdate({});
  }

  load_stream_started_ = false;
}

int SurfaceUpdater::GetSliceIndexFromSliceId(const std::string& slice_id) {
  ContentRevision slice_rev = ToContentRevision(slice_id);
  if (slice_rev.is_null() || !model_)
    return -1;
  int index = 0;
  for (const ContentRevision& rev : model_->GetContentList()) {
    if (rev == slice_rev)
      return index;
    ++index;
  }
  return -1;
}

void SurfaceUpdater::SetLoadingMore(bool is_loading) {
  DCHECK(!loading_initial_)
      << "SetLoadingMore while still loading the initial state";
  loading_more_ = is_loading;
  if (loading_more_) {
    current_load_more_indicator_id_++;
  }
  SendStreamUpdateIfNeeded();
}

DrawState SurfaceUpdater::GetState() const {
  DrawState new_state;
  new_state.loading_more = loading_more_;
  new_state.loading_initial = loading_initial_;
  if (load_stream_failed_)
    new_state.zero_state_type = GetZeroStateType(load_stream_status_);
  return new_state;
}

bool SurfaceUpdater::ShouldSendStreamUpdate() const {
  return !(last_draw_state_ == GetState());
}

void SurfaceUpdater::SendStreamUpdateIfNeeded() {
  if (ShouldSendStreamUpdate())
    SendStreamUpdate({});
}

void SurfaceUpdater::SendStreamUpdate(
    const std::vector<std::string>& updated_shared_state_ids) {
  DrawState state = GetState();
  StreamUpdateAndType update =
      MakeStreamUpdate(updated_shared_state_ids, sent_content_, model_, state,
                       current_load_more_indicator_id_);

  if (load_stream_started_ || loading_more_) {
    launch_reliability_logger_.OnStreamUpdate(update.type);
  }

  for (auto& entry : *surfaces_) {
    SendUpdateToSurface(entry.surface_id, entry.renderer, update.stream_update);
  }

  sent_content_ = GetContentSet(model_);
  last_draw_state_ = state;
}

void SurfaceUpdater::SendUpdateToSurface(SurfaceId surface_id,
                                         SurfaceRenderer* renderer,
                                         const feedui::StreamUpdate& update) {
  renderer->StreamUpdate(update);

  // Call |MetricsReporter::SurfaceReceivedContent()| if appropriate.

  bool update_has_content = false;
  for (const feedui::StreamUpdate_SliceUpdate& slice_update :
       update.updated_slices()) {
    if (slice_update.has_slice() && slice_update.slice().has_xsurface_slice()) {
      update_has_content = true;
    }
  }
  if (!update_has_content)
    return;
  metrics_reporter_->SurfaceReceivedContent(surface_id);
}

void SurfaceUpdater::SetOfflinePageAvailability(const std::string& badge_id,
                                                bool available_offline) {
  feedxsurface::OfflineBadgeContent testbadge;
  if (available_offline) {
    std::string badge_serialized;
    testbadge.set_available_offline(available_offline);
    testbadge.SerializeToString(&badge_serialized);
    surface_data_slice_.UpdateDatastoreEntry(badge_id, badge_serialized);
  } else {
    surface_data_slice_.RemoveDatastoreEntry(badge_id);
  }
}

}  // namespace feed
