// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERFORMANCE_MANAGER_PERSISTENCE_SITE_DATA_SITE_DATA_IMPL_H_
#define COMPONENTS_PERFORMANCE_MANAGER_PERSISTENCE_SITE_DATA_SITE_DATA_IMPL_H_

#include <optional>
#include <utility>
#include <vector>

#include "base/functional/callback_forward.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "components/performance_manager/persistence/site_data/exponential_moving_average.h"
#include "components/performance_manager/persistence/site_data/site_data.pb.h"
#include "components/performance_manager/persistence/site_data/site_data_store.h"
#include "components/performance_manager/persistence/site_data/tab_visibility.h"
#include "components/performance_manager/public/persistence/site_data/feature_usage.h"
#include "url/origin.h"

namespace performance_manager {

class SiteDataCacheImpl;
class SiteDataReaderTest;
class SiteDataWriterTest;
class MockDataCache;

FORWARD_DECLARE_TEST(SiteDataReaderTest,
                     DestroyingReaderCancelsPendingCallbacks);
FORWARD_DECLARE_TEST(SiteDataReaderTest,
                     FreeingReaderDoesntCauseWriteOperation);
FORWARD_DECLARE_TEST(SiteDataReaderTest, OnDataLoadedCallbackInvoked);

namespace internal {

FORWARD_DECLARE_TEST(SiteDataImplTest, LateAsyncReadDoesntBypassClearEvent);

// Internal class used to read/write site data. This is a wrapper class around
// a SiteDataProto object and offers various to query and/or modify
// it. This class shouldn't be used directly, instead it should be created by
// a LocalSiteCharacteristicsDataStore that will serve reader and writer
// objects.
//
// Reader and writers objects that are interested in reading/writing information
// about the same origin will share a unique ref counted instance of this
// object, because of this all the operations done on these objects should be
// done on the same thread, this class isn't thread safe.
//
// By default tabs associated with instances of this class are assumed to be
// running in foreground, |NotifyTabBackgrounded| should get called to indicate
// that the tab is running in background.
class SiteDataImpl : public base::RefCounted<SiteDataImpl> {
 public:
  // Interface that should be implemented in order to receive notifications when
  // this object is about to get destroyed.
  class OnDestroyDelegate {
   public:
    // Called when this object is about to get destroyed.
    virtual void OnSiteDataImplDestroyed(SiteDataImpl* impl) = 0;
  };

  SiteDataImpl(const SiteDataImpl&) = delete;
  SiteDataImpl& operator=(const SiteDataImpl&) = delete;

  // Must be called when a load event is received for this site, this can be
  // invoked several times if instances of this class are shared between
  // multiple tabs.
  void NotifySiteLoaded();

  // Must be called when an unload event is received for this site, this can be
  // invoked several times if instances of this class are shared between
  // multiple tabs.
  void NotifySiteUnloaded(TabVisibility tab_visibility);

  // Must be called when a loaded tab gets backgrounded.
  void NotifyLoadedSiteBackgrounded();

  // Must be called when a loaded tab gets foregrounded.
  void NotifyLoadedSiteForegrounded();

  // Returns the usage of a given feature for this origin.
  SiteFeatureUsage UpdatesFaviconInBackground() const;
  SiteFeatureUsage UpdatesTitleInBackground() const;
  SiteFeatureUsage UsesAudioInBackground() const;

  // Returns true if the most authoritative data has been loaded from the
  // backing store.
  bool DataLoaded() const;

  // Registers a callback to be invoked when the data backing this object is
  // loaded from disk, or otherwise authoritatively initialized.
  void RegisterDataLoadedCallback(base::OnceClosure&& callback);

  // Accessors for load-time performance measurement estimates.
  // If |num_datum| is zero, there's no estimate available.
  const ExponentialMovingAverage& load_duration() const {
    return load_duration_;
  }
  const ExponentialMovingAverage& cpu_usage_estimate() const {
    return cpu_usage_estimate_;
  }
  const ExponentialMovingAverage& private_footprint_kb_estimate() const {
    return private_footprint_kb_estimate_;
  }

  // Must be called when a feature is used, calling this function updates the
  // last observed timestamp for this feature.
  void NotifyUpdatesFaviconInBackground();
  void NotifyUpdatesTitleInBackground();
  void NotifyUsesAudioInBackground();

  // Call when a load-time performance measurement becomes available.
  void NotifyLoadTimePerformanceMeasurement(
      base::TimeDelta load_duration,
      base::TimeDelta cpu_usage_estimate,
      uint64_t private_footprint_kb_estimate);

  base::TimeDelta last_loaded_time_for_testing() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return InternalRepresentationToTimeDelta(
        site_characteristics_.last_loaded());
  }

  const SiteDataProto& site_characteristics_for_testing() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return site_characteristics_;
  }

  size_t loaded_tabs_count_for_testing() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return loaded_tabs_count_;
  }

  size_t loaded_tabs_in_background_count_for_testing() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return loaded_tabs_in_background_count_;
  }

  base::TimeTicks background_session_begin_for_testing() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return background_session_begin_;
  }

  const url::Origin& origin() const { return origin_; }
  bool is_dirty() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return is_dirty_;
  }

  void ExpireAllObservationWindowsForTesting();

  void ClearObservationsAndInvalidateReadOperationForTesting() {
    ClearObservationsAndInvalidateReadOperation();
  }

  bool fully_initialized_for_testing() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return fully_initialized_;
  }

  static const base::TimeDelta GetFeatureObservationWindowLengthForTesting();

 protected:
  friend class base::RefCounted<SiteDataImpl>;
  friend class performance_manager::SiteDataCacheImpl;

  // Friend all the tests.
  friend class SiteDataImplTest;
  friend class performance_manager::SiteDataReaderTest;
  friend class performance_manager::SiteDataWriterTest;
  friend class performance_manager::MockDataCache;

  SiteDataImpl(const url::Origin& origin,
               base::WeakPtr<OnDestroyDelegate> delegate,
               SiteDataStore* data_store);

  virtual ~SiteDataImpl();

  // Helper functions to convert from/to the internal representation that is
  // used to store TimeDelta values in the |SiteDataProto| protobuf.
  static base::TimeDelta InternalRepresentationToTimeDelta(
      ::google::protobuf::int64 value) {
    return base::Seconds(value);
  }
  static int64_t TimeDeltaToInternalRepresentation(base::TimeDelta delta) {
    return delta.InSeconds();
  }

  // Returns for how long a given feature has been observed, this is the sum of
  // the recorded observation duration and the current observation duration
  // since this site has been loaded (if applicable). If a feature has been
  // used then it returns 0.
  base::TimeDelta FeatureObservationDuration(
      const SiteDataFeatureProto& feature_proto) const;

 private:
  FRIEND_TEST_ALL_PREFIXES(SiteDataImplTest,
                           FlushingStateToProtoDoesntAffectData);
  FRIEND_TEST_ALL_PREFIXES(SiteDataImplTest,
                           LateAsyncReadDoesntBypassClearEvent);
  FRIEND_TEST_ALL_PREFIXES(performance_manager::SiteDataReaderTest,
                           DestroyingReaderCancelsPendingCallbacks);
  FRIEND_TEST_ALL_PREFIXES(performance_manager::SiteDataReaderTest,
                           FreeingReaderDoesntCauseWriteOperation);
  FRIEND_TEST_ALL_PREFIXES(performance_manager::SiteDataReaderTest,
                           OnDataLoadedCallbackInvoked);

  // Add |extra_observation_duration| to the observation window of a given
  // feature if it hasn't been used yet, do nothing otherwise.
  static void IncrementFeatureObservationDuration(
      SiteDataFeatureProto* feature_proto,
      base::TimeDelta extra_observation_duration);

  // Clear all the past observations about this site and invalidate the pending
  // read observations from the data store.
  void ClearObservationsAndInvalidateReadOperation();

  // Returns the usage of |site_feature| for this site.
  SiteFeatureUsage GetFeatureUsage(
      const SiteDataFeatureProto& feature_proto) const;

  // Helper function to update a given |SiteDataFeatureProto| when a
  // feature gets used.
  void NotifyFeatureUsage(SiteDataFeatureProto* feature_proto,
                          const char* feature_name);

  bool IsLoaded() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return loaded_tabs_count_ > 0U;
  }

  // Callback that needs to be called by the data store once it has finished
  // trying to read the protobuf.
  void OnInitCallback(std::optional<SiteDataProto> site_characteristic_proto);

  // Decrement the |loaded_tabs_in_background_count_| counter and update the
  // local feature observation durations if necessary.
  void DecrementNumLoadedBackgroundTabs();

  // Flush any state that's maintained in member variables to the proto.
  const SiteDataProto& FlushStateToProto();

  // Updates the proto with the current total observation duration and updates
  // |background_session_begin_| to NowTicks().
  void FlushFeaturesObservationDurationToProto();

  void TransitionToFullyInitialized();

  // This site's characteristics, contains the features and other values are
  // measured.
  SiteDataProto site_characteristics_ GUARDED_BY_CONTEXT(sequence_checker_);

  // The in-memory storage for the moving performance averages.
  ExponentialMovingAverage load_duration_
      GUARDED_BY_CONTEXT(sequence_checker_);  // microseconds.
  ExponentialMovingAverage cpu_usage_estimate_
      GUARDED_BY_CONTEXT(sequence_checker_);  // microseconds.
  ExponentialMovingAverage private_footprint_kb_estimate_
      GUARDED_BY_CONTEXT(sequence_checker_);

  // This site's origin.
  const url::Origin origin_;

  // The number of loaded tabs for this origin. Several tabs with the
  // same origin might share the same instance of this object, this counter
  // will allow to properly update the observation time (starts when the first
  // tab gets loaded, stops when the last one gets unloaded).
  size_t loaded_tabs_count_ GUARDED_BY_CONTEXT(sequence_checker_);

  // Number of loaded tabs currently in background for this origin, the
  // implementation doesn't need to track unloaded tabs running in background.
  size_t loaded_tabs_in_background_count_ GUARDED_BY_CONTEXT(sequence_checker_);

  // The time at which the |loaded_tabs_in_background_count_| counter changed
  // from 0 to 1.
  base::TimeTicks background_session_begin_
      GUARDED_BY_CONTEXT(sequence_checker_);

  // The data store used to store the site characteristics, it should outlive
  // this object.
  const raw_ptr<SiteDataStore, DanglingUntriaged> data_store_;

  // The delegate that should get notified when this object is about to get
  // destroyed, it should outlive this object.
  // The use of WeakPtr here is a temporary, minimally invasive fix for the UAF
  // reported in https://crbug.com/1231933. By using a WeakPtr, the call-out
  // is avoided in the case where the OnDestroyDelegate has been deleted before
  // all SiteDataImpls have been released.
  // The proper fix for this is going to be more invasive and less suitable
  // for merging, should it come to that.
  base::WeakPtr<OnDestroyDelegate> const delegate_;

  // Indicates if this object has been fully initialized, either because the
  // read operation from the database has completed or because it has been
  // cleared.
  bool fully_initialized_ GUARDED_BY_CONTEXT(sequence_checker_);

  // Dirty bit, indicates if any of the fields in |site_characteristics_| has
  // changed since it has been initialized.
  bool is_dirty_ GUARDED_BY_CONTEXT(sequence_checker_);

  // A collection of callbacks to be invoked when this object becomes fully
  // initialized.
  std::vector<base::OnceClosure> data_loaded_callbacks_
      GUARDED_BY_CONTEXT(sequence_checker_);

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<SiteDataImpl> weak_factory_
      GUARDED_BY_CONTEXT(sequence_checker_){this};
};

}  // namespace internal
}  // namespace performance_manager

#endif  // COMPONENTS_PERFORMANCE_MANAGER_PERSISTENCE_SITE_DATA_SITE_DATA_IMPL_H_
