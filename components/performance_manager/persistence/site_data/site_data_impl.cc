// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/persistence/site_data/site_data_impl.h"

#include <algorithm>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"

namespace performance_manager {
namespace internal {

namespace {

// The sample weighing factor for the exponential moving averages for
// performance measurements. A factor of 1/2 gives each sample an equal weight
// to the entire previous history. As we don't know much noise there is to the
// measurement, this is essentially a shot in the dark.
// TODO(siggi): Consider adding UMA metrics to capture e.g. the fractional delta
//      from the current average, or some such.
constexpr float kSampleWeightFactor = 0.5;

base::TimeDelta GetTickDeltaSinceEpoch() {
  return base::TimeTicks::Now() - base::TimeTicks::UnixEpoch();
}

// Returns all the SiteDataFeatureProto elements contained in a
// SiteDataProto protobuf object.
std::vector<SiteDataFeatureProto*> GetAllFeaturesFromProto(
    SiteDataProto* proto) {
  std::vector<SiteDataFeatureProto*> ret(
      {proto->mutable_updates_favicon_in_background(),
       proto->mutable_updates_title_in_background(),
       proto->mutable_uses_audio_in_background()});

  return ret;
}

// Observations windows have a default value of 2 hours, 95% of backgrounded
// tabs don't use any of these features in this time window.
static constexpr base::TimeDelta kObservationWindowLength = base::Hours(2);

}  // namespace

void SiteDataImpl::NotifySiteLoaded() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Update the last loaded time when this origin gets loaded for the first
  // time.
  if (loaded_tabs_count_ == 0) {
    site_characteristics_.set_last_loaded(
        TimeDeltaToInternalRepresentation(GetTickDeltaSinceEpoch()));

    is_dirty_ = true;
  }
  loaded_tabs_count_++;
}

void SiteDataImpl::NotifySiteUnloaded(TabVisibility tab_visibility) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (tab_visibility == TabVisibility::kBackground)
    DecrementNumLoadedBackgroundTabs();

  DCHECK_GT(loaded_tabs_count_, 0U);
  loaded_tabs_count_--;
  // Only update the last loaded time when there's no more loaded instance of
  // this origin.
  if (loaded_tabs_count_ > 0U)
    return;

  base::TimeDelta current_unix_time = GetTickDeltaSinceEpoch();

  // Update the |last_loaded_time_| field, as the moment this site gets unloaded
  // also corresponds to the last moment it was loaded.
  site_characteristics_.set_last_loaded(
      TimeDeltaToInternalRepresentation(current_unix_time));
}

void SiteDataImpl::NotifyLoadedSiteBackgrounded() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (loaded_tabs_in_background_count_ == 0)
    background_session_begin_ = base::TimeTicks::Now();

  loaded_tabs_in_background_count_++;

  DCHECK_LE(loaded_tabs_in_background_count_, loaded_tabs_count_);
}

void SiteDataImpl::NotifyLoadedSiteForegrounded() {
  DecrementNumLoadedBackgroundTabs();
}

SiteFeatureUsage SiteDataImpl::UpdatesFaviconInBackground() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return GetFeatureUsage(site_characteristics_.updates_favicon_in_background());
}

SiteFeatureUsage SiteDataImpl::UpdatesTitleInBackground() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return GetFeatureUsage(site_characteristics_.updates_title_in_background());
}

SiteFeatureUsage SiteDataImpl::UsesAudioInBackground() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return GetFeatureUsage(site_characteristics_.uses_audio_in_background());
}

bool SiteDataImpl::DataLoaded() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return fully_initialized_;
}

void SiteDataImpl::RegisterDataLoadedCallback(base::OnceClosure&& callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (fully_initialized_) {
    std::move(callback).Run();
    return;
  }
  data_loaded_callbacks_.emplace_back(std::move(callback));
}

void SiteDataImpl::NotifyUpdatesFaviconInBackground() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  NotifyFeatureUsage(
      site_characteristics_.mutable_updates_favicon_in_background(),
      "FaviconUpdateInBackground");
}

void SiteDataImpl::NotifyUpdatesTitleInBackground() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  NotifyFeatureUsage(
      site_characteristics_.mutable_updates_title_in_background(),
      "TitleUpdateInBackground");
}

void SiteDataImpl::NotifyUsesAudioInBackground() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  NotifyFeatureUsage(site_characteristics_.mutable_uses_audio_in_background(),
                     "AudioUsageInBackground");
}

void SiteDataImpl::NotifyLoadTimePerformanceMeasurement(
    base::TimeDelta load_duration,
    base::TimeDelta cpu_usage_estimate,
    uint64_t private_footprint_kb_estimate) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  is_dirty_ = true;

  load_duration_.AppendDatum(load_duration.InMicroseconds());
  cpu_usage_estimate_.AppendDatum(cpu_usage_estimate.InMicroseconds());
  private_footprint_kb_estimate_.AppendDatum(private_footprint_kb_estimate);
}

void SiteDataImpl::ExpireAllObservationWindowsForTesting() {
  for (auto* iter : GetAllFeaturesFromProto(&site_characteristics_))
    IncrementFeatureObservationDuration(iter, kObservationWindowLength);
}

// static
const base::TimeDelta
SiteDataImpl::GetFeatureObservationWindowLengthForTesting() {
  return kObservationWindowLength;
}

SiteDataImpl::SiteDataImpl(const url::Origin& origin,
                           base::WeakPtr<OnDestroyDelegate> delegate,
                           SiteDataStore* data_store)
    : load_duration_(kSampleWeightFactor),
      cpu_usage_estimate_(kSampleWeightFactor),
      private_footprint_kb_estimate_(kSampleWeightFactor),
      origin_(origin),
      loaded_tabs_count_(0U),
      loaded_tabs_in_background_count_(0U),
      data_store_(data_store),
      delegate_(delegate),
      fully_initialized_(false),
      is_dirty_(false) {
  DCHECK(data_store_);
  DCHECK(delegate_);

  data_store_->ReadSiteDataFromStore(
      origin_, base::BindOnce(&SiteDataImpl::OnInitCallback,
                              weak_factory_.GetWeakPtr()));
}

SiteDataImpl::~SiteDataImpl() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // All users of this object should make sure that they send the same number of
  // NotifySiteLoaded and NotifySiteUnloaded events, in practice this mean
  // tracking the loaded state and sending an unload event in their destructor
  // if needed.
  DCHECK(!IsLoaded());
  DCHECK_EQ(0U, loaded_tabs_in_background_count_);

  // Make sure not to dispatch a notification to a deleted delegate, and gate
  // the DB write on it too, as the delegate and the data store have the
  // same lifetime.
  // TODO(crbug.com/40056631): Fix this properly and restore the end of
  //     life write here.
  if (delegate_) {
    delegate_->OnSiteDataImplDestroyed(this);

    // TODO(sebmarchand): Some data might be lost here if the read operation has
    // not completed, add some metrics to measure if this is really an issue.
    if (is_dirty_ && fully_initialized_) {
      // SiteDataImpl is only created from SiteDataCacheImpl, not from the
      // NonRecordingSiteDataCache that's used for OTR profiles, so this should
      // always be logged.
      base::UmaHistogramBoolean(
          "PerformanceManager.SiteDB.WriteScheduled.WriteSiteDataIntoStore",
          true);
      data_store_->WriteSiteDataIntoStore(origin_, FlushStateToProto());
    }
  }
}

base::TimeDelta SiteDataImpl::FeatureObservationDuration(
    const SiteDataFeatureProto& feature_proto) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Get the current observation duration value if available.
  base::TimeDelta observation_time_for_feature;
  if (feature_proto.has_observation_duration()) {
    observation_time_for_feature =
        InternalRepresentationToTimeDelta(feature_proto.observation_duration());
  }

  // If this site is still in background and the feature isn't in use then the
  // observation time since load needs to be added.
  if (loaded_tabs_in_background_count_ > 0U &&
      InternalRepresentationToTimeDelta(feature_proto.use_timestamp())
          .is_zero()) {
    base::TimeDelta observation_time_since_backgrounded =
        base::TimeTicks::Now() - background_session_begin_;
    observation_time_for_feature += observation_time_since_backgrounded;
  }

  return observation_time_for_feature;
}

// static:
void SiteDataImpl::IncrementFeatureObservationDuration(
    SiteDataFeatureProto* feature_proto,
    base::TimeDelta extra_observation_duration) {
  if (!feature_proto->has_use_timestamp() ||
      InternalRepresentationToTimeDelta(feature_proto->use_timestamp())
          .is_zero()) {
    feature_proto->set_observation_duration(TimeDeltaToInternalRepresentation(
        InternalRepresentationToTimeDelta(
            feature_proto->observation_duration()) +
        extra_observation_duration));
  }
}

void SiteDataImpl::ClearObservationsAndInvalidateReadOperation() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Invalidate the weak pointer that have been served, this will ensure that
  // this object doesn't get initialized from the data store after being
  // cleared.
  weak_factory_.InvalidateWeakPtrs();

  // Reset all the observations.
  site_characteristics_.Clear();

  // Clear the performance estimates, both the local state and the proto.
  cpu_usage_estimate_.Clear();
  private_footprint_kb_estimate_.Clear();
  site_characteristics_.clear_load_time_estimates();

  // Set the last loaded time to the current time if there's some loaded
  // instances of this site.
  if (IsLoaded()) {
    site_characteristics_.set_last_loaded(
        TimeDeltaToInternalRepresentation(GetTickDeltaSinceEpoch()));
  }

  // This object is now in a valid state and can be written in the data store.
  TransitionToFullyInitialized();
}

SiteFeatureUsage SiteDataImpl::GetFeatureUsage(
    const SiteDataFeatureProto& feature_proto) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  base::UmaHistogramBoolean(
      "PerformanceManager.SiteDB.ReadHasCompletedBeforeQuery",
      fully_initialized_);

  // Checks if this feature has already been observed.
  // TODO(sebmarchand): Check the timestamp and reset features that haven't been
  // observed in a long time, https://crbug.com/826446.
  if (feature_proto.has_use_timestamp())
    return SiteFeatureUsage::kSiteFeatureInUse;

  if (FeatureObservationDuration(feature_proto) >= kObservationWindowLength)
    return SiteFeatureUsage::kSiteFeatureNotInUse;

  return SiteFeatureUsage::kSiteFeatureUsageUnknown;
}

void SiteDataImpl::NotifyFeatureUsage(SiteDataFeatureProto* feature_proto,
                                      const char* feature_name) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(IsLoaded());
  DCHECK_GT(loaded_tabs_in_background_count_, 0U);

  // Report the observation time if this is the first time this feature is
  // observed.
  if (feature_proto->observation_duration() != 0) {
    base::UmaHistogramCustomTimes(
        base::StrCat(
            {"PerformanceManager.SiteDB.ObservationTimeBeforeFirstUse.",
             feature_name}),
        InternalRepresentationToTimeDelta(
            feature_proto->observation_duration()),
        base::Seconds(1), base::Days(1), 100);
  }

  feature_proto->Clear();
  feature_proto->set_use_timestamp(
      TimeDeltaToInternalRepresentation(GetTickDeltaSinceEpoch()));
}

void SiteDataImpl::OnInitCallback(
    std::optional<SiteDataProto> db_site_characteristics) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Check if the initialization has succeeded.
  if (db_site_characteristics) {
    // If so, iterates over all the features and initialize them.
    auto this_features = GetAllFeaturesFromProto(&site_characteristics_);
    auto db_features =
        GetAllFeaturesFromProto(&db_site_characteristics.value());
    auto this_features_iter = this_features.begin();
    auto db_features_iter = db_features.begin();
    for (; this_features_iter != this_features.end() &&
           db_features_iter != db_features.end();
         ++this_features_iter, ++db_features_iter) {
      // If the |use_timestamp| field is set for the in-memory entry for this
      // feature then there's nothing to do, otherwise update it with the values
      // from the data store.
      if (!(*this_features_iter)->has_use_timestamp()) {
        if ((*db_features_iter)->has_use_timestamp() &&
            (*db_features_iter)->use_timestamp() != 0) {
          (*this_features_iter)->Clear();
          // Keep the use timestamp from the data store, if any.
          (*this_features_iter)
              ->set_use_timestamp((*db_features_iter)->use_timestamp());
        } else {
          // Else, add the observation duration from the data store to the
          // in-memory observation duration.
          IncrementFeatureObservationDuration(
              (*this_features_iter),
              InternalRepresentationToTimeDelta(
                  (*db_features_iter)->observation_duration()));
        }
      }
    }
    // Only update the last loaded field if we haven't updated it since the
    // creation of this object.
    if (!site_characteristics_.has_last_loaded()) {
      site_characteristics_.set_last_loaded(
          db_site_characteristics->last_loaded());
    }
    // If there was on-disk data, update the in-memory performance averages.
    if (db_site_characteristics->has_load_time_estimates()) {
      const auto& estimates = db_site_characteristics->load_time_estimates();
      if (estimates.has_avg_load_duration_us())
        load_duration_.PrependDatum(estimates.avg_load_duration_us());
      if (estimates.has_avg_cpu_usage_us())
        cpu_usage_estimate_.PrependDatum(estimates.avg_cpu_usage_us());
      if (estimates.has_avg_footprint_kb()) {
        private_footprint_kb_estimate_.PrependDatum(
            estimates.avg_footprint_kb());
      }
    }
  }

  TransitionToFullyInitialized();
}

void SiteDataImpl::DecrementNumLoadedBackgroundTabs() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_GT(loaded_tabs_in_background_count_, 0U);
  loaded_tabs_in_background_count_--;
  // Only update the observation durations if there's no more backgounded
  // instance of this origin.
  if (loaded_tabs_in_background_count_ == 0U)
    FlushFeaturesObservationDurationToProto();
}

const SiteDataProto& SiteDataImpl::FlushStateToProto() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Update the proto with the most current performance measurement averages.
  if (cpu_usage_estimate_.num_datums() ||
      private_footprint_kb_estimate_.num_datums()) {
    auto* estimates = site_characteristics_.mutable_load_time_estimates();
    if (load_duration_.num_datums())
      estimates->set_avg_load_duration_us(load_duration_.value());
    if (cpu_usage_estimate_.num_datums())
      estimates->set_avg_cpu_usage_us(cpu_usage_estimate_.value());
    if (private_footprint_kb_estimate_.num_datums()) {
      estimates->set_avg_footprint_kb(private_footprint_kb_estimate_.value());
    }
  }

  if (loaded_tabs_in_background_count_ > 0U)
    FlushFeaturesObservationDurationToProto();

  return site_characteristics_;
}

void SiteDataImpl::FlushFeaturesObservationDurationToProto() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!background_session_begin_.is_null());

  base::TimeTicks now = base::TimeTicks::Now();

  base::TimeDelta extra_observation_duration = now - background_session_begin_;
  background_session_begin_ = now;

  // Update the observation duration fields.
  for (auto* iter : GetAllFeaturesFromProto(&site_characteristics_))
    IncrementFeatureObservationDuration(iter, extra_observation_duration);
}

void SiteDataImpl::TransitionToFullyInitialized() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  fully_initialized_ = true;
  for (size_t i = 0; i < data_loaded_callbacks_.size(); ++i)
    std::move(data_loaded_callbacks_[i]).Run();
  data_loaded_callbacks_.clear();
}

}  // namespace internal
}  // namespace performance_manager
