// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/call_stacks/call_stack_profile_metrics_provider.h"

#include <utility>
#include <vector>

#include "base/check.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/no_destructor.h"
#include "base/ranges/algorithm.h"
#include "base/synchronization/lock.h"
#include "base/thread_annotations.h"
#include "base/time/time.h"
#include "base/types/optional_util.h"
#include "components/metrics/public/mojom/call_stack_profile_collector.mojom.h"
#include "mojo/public/cpp/base/proto_wrapper.h"
#include "third_party/metrics_proto/chrome_user_metrics_extension.pb.h"
#include "third_party/metrics_proto/sampled_profile.pb.h"

namespace metrics {

namespace {

constexpr base::FeatureState kSamplingProfilerReportingDefaultState =
    base::FEATURE_ENABLED_BY_DEFAULT;

bool SamplingProfilerReportingEnabled() {
  // TODO(crbug.com/40246378): Do not call this function before the FeatureList
  // is registered.
  if (!base::FeatureList::GetInstance()) {
    // The FeatureList is not registered: use the feature's default state. This
    // means that any override from the command line or variations service is
    // ignored.
    return kSamplingProfilerReportingDefaultState ==
           base::FEATURE_ENABLED_BY_DEFAULT;
  }
  return base::FeatureList::IsEnabled(kSamplingProfilerReporting);
}

// Cap the number of pending profiles to avoid excessive performance overhead
// due to profile deserialization when profile uploads are delayed (e.g. due to
// being offline). Capping at this threshold loses approximately 0.5% of
// profiles on canary and dev.
//
// TODO(wittman): Remove this threshold after crbug.com/903972 is fixed.
const size_t kMaxPendingProfiles = 1250;

// Provides access to the singleton interceptor callback instance for CPU
// profiles. Accessed asynchronously on the profiling thread after profiling has
// been started.
CallStackProfileMetricsProvider::InterceptorCallback&
GetCpuInterceptorCallbackInstance() {
  static base::NoDestructor<
      CallStackProfileMetricsProvider::InterceptorCallback>
      instance;
  return *instance;
}

// PendingProfiles ------------------------------------------------------------

// Singleton class responsible for retaining profiles received from
// CallStackProfileBuilder. These are then sent to UMA on the invocation of
// CallStackProfileMetricsProvider::ProvideCurrentSessionData(). We need to
// store the profiles outside of a CallStackProfileMetricsProvider instance
// since callers may start profiling before the CallStackProfileMetricsProvider
// is created.
//
// Member functions on this class may be called on any thread.
class PendingProfiles {
 public:
  static PendingProfiles* GetInstance();

  PendingProfiles(const PendingProfiles&) = delete;
  PendingProfiles& operator=(const PendingProfiles&) = delete;

  // Retrieves all the pending profiles.
  std::vector<SampledProfile> RetrieveProfiles();

  // Enables the collection of profiles by MaybeCollect*Profile if |enabled| is
  // true. Otherwise, clears the currently collected profiles and ignores
  // profiles provided to future invocations of MaybeCollect*Profile.
  void SetCollectionEnabled(bool enabled);

  // Collects |profile|. It may be stored in a serialized form, or ignored,
  // depending on the pre-defined storage capacity and whether collection is
  // enabled. |profile| is not const& because it must be passed with std::move.
  void MaybeCollectProfile(base::TimeTicks profile_start_time,
                           SampledProfile profile);

  // Collects |serialized_profile|. It may be ignored depending on the
  // pre-defined storage capacity and whether collection is enabled.
  void MaybeCollectSerializedProfile(
      base::TimeTicks profile_start_time,
      mojom::SampledProfilePtr serialized_profile);

#if BUILDFLAG(IS_CHROMEOS)
  // Returns all the serialized profiles that have been collected but not yet
  // retrieved. For thread-safety reasons, deserializes under a lock, so this is
  // an expensive function. Fortunately, it's only called during ChromeOS tast
  // integration tests.
  std::vector<SampledProfile> GetUnretrievedProfiles() {
    base::AutoLock scoped_lock(lock_);
    std::vector<SampledProfile> profiles;
    profiles.reserve(serialized_profiles_.size());
    for (const mojom::SampledProfilePtr& serialized_profile :
         serialized_profiles_) {
      SampledProfile profile;
      if (base::OptionalUnwrapTo(
              serialized_profile->contents.As<SampledProfile>(), profile)) {
        profiles.push_back(std::move(profile));
      }
    }
    return profiles;
  }
#endif  // BUILDFLAG(IS_CHROMEOS)

  // Allows testing against the initial state multiple times.
  void ResetToDefaultStateForTesting();

 private:
  friend class base::NoDestructor<PendingProfiles>;

  PendingProfiles();
  ~PendingProfiles() = delete;

  // Returns true if collection is enabled for a given profile based on its
  // |profile_start_time|. The |lock_| must be held prior to calling this
  // method.
  bool IsCollectionEnabledForProfile(base::TimeTicks profile_start_time) const
      EXCLUSIVE_LOCKS_REQUIRED(lock_);

  mutable base::Lock lock_;

  // If true, profiles provided to MaybeCollect*Profile should be collected.
  // Otherwise they will be ignored.
  // |collection_enabled_| is initialized to true to collect any profiles that
  // are generated prior to creation of the CallStackProfileMetricsProvider.
  // The ultimate disposition of these pre-creation collected profiles will be
  // determined by the initial recording state provided to
  // CallStackProfileMetricsProvider.
  bool collection_enabled_ GUARDED_BY(lock_) = true;

  // The last time collection was disabled. Used to determine if collection was
  // disabled at any point since a profile was started.
  base::TimeTicks last_collection_disable_time_ GUARDED_BY(lock_);

  // The last time collection was enabled. Used to determine if collection was
  // enabled at any point since a profile was started.
  base::TimeTicks last_collection_enable_time_ GUARDED_BY(lock_);

  // The set of completed serialized profiles that should be reported.
  std::vector<mojom::SampledProfilePtr> serialized_profiles_ GUARDED_BY(lock_);
};

// static
PendingProfiles* PendingProfiles::GetInstance() {
  // Singleton for performance rather than correctness reasons.
  static base::NoDestructor<PendingProfiles> instance;
  return instance.get();
}

std::vector<SampledProfile> PendingProfiles::RetrieveProfiles() {
  std::vector<mojom::SampledProfilePtr> serialized_profiles;

  {
    base::AutoLock scoped_lock(lock_);
    serialized_profiles.swap(serialized_profiles_);
  }

  // Deserialize all serialized profiles, skipping over any that fail to parse.
  std::vector<SampledProfile> profiles;
  profiles.reserve(serialized_profiles.size());
  for (const mojom::SampledProfilePtr& serialized_profile :
       serialized_profiles) {
    SampledProfile profile;
    if (base::OptionalUnwrapTo(
            serialized_profile->contents.As<SampledProfile>(), profile)) {
      profiles.push_back(std::move(profile));
    }
  }

  return profiles;
}

void PendingProfiles::SetCollectionEnabled(bool enabled) {
  base::AutoLock scoped_lock(lock_);

  collection_enabled_ = enabled;

  if (!collection_enabled_) {
    serialized_profiles_.clear();
    last_collection_disable_time_ = base::TimeTicks::Now();
  } else {
    last_collection_enable_time_ = base::TimeTicks::Now();
  }
}

bool PendingProfiles::IsCollectionEnabledForProfile(
    base::TimeTicks profile_start_time) const {
  lock_.AssertAcquired();

  // Scenario 1: return false if collection is disabled.
  if (!collection_enabled_)
    return false;

  // Scenario 2: return false if collection is disabled after the start of
  // collection for this profile.
  if (!last_collection_disable_time_.is_null() &&
      last_collection_disable_time_ >= profile_start_time) {
    return false;
  }

  // Scenario 3: return false if collection is disabled before the start of
  // collection and re-enabled after the start. Note that this is different from
  // scenario 1 where re-enabling never happens.
  if (!last_collection_disable_time_.is_null() &&
      !last_collection_enable_time_.is_null() &&
      last_collection_enable_time_ >= profile_start_time) {
    return false;
  }

  return true;
}

void PendingProfiles::MaybeCollectProfile(base::TimeTicks profile_start_time,
                                          SampledProfile profile) {
  {
    base::AutoLock scoped_lock(lock_);

    if (!IsCollectionEnabledForProfile(profile_start_time))
      return;
  }

  // Serialize the profile without holding the lock.
  mojom::SampledProfilePtr serialized_profile = mojom::SampledProfile::New();
  serialized_profile->contents = mojo_base::ProtoWrapper(profile);

  MaybeCollectSerializedProfile(profile_start_time,
                                std::move(serialized_profile));
}

void PendingProfiles::MaybeCollectSerializedProfile(
    base::TimeTicks profile_start_time,
    mojom::SampledProfilePtr serialized_profile) {
  base::AutoLock scoped_lock(lock_);

  // There is no room for additional profiles.
  if (serialized_profiles_.size() >= kMaxPendingProfiles)
    return;

  if (IsCollectionEnabledForProfile(profile_start_time))
    serialized_profiles_.push_back(std::move(serialized_profile));
}

void PendingProfiles::ResetToDefaultStateForTesting() {
  base::AutoLock scoped_lock(lock_);

  collection_enabled_ = true;
  last_collection_disable_time_ = base::TimeTicks();
  last_collection_enable_time_ = base::TimeTicks();
  serialized_profiles_.clear();
}

PendingProfiles::PendingProfiles() = default;

#if BUILDFLAG(IS_CHROMEOS)
// A class that records the number of minimally-successful profiles received
// over time. In ChromeOS, this is used by the ui.StackSampledMetrics tast
// integration test to confirm that stack-sampled metrics are working on
// all the various ChromeOS boards.
class ReceivedProfileCounter {
 public:
  static ReceivedProfileCounter* GetInstance();

  ReceivedProfileCounter(const ReceivedProfileCounter&) = delete;
  ReceivedProfileCounter& operator=(const ReceivedProfileCounter&) = delete;
  ~ReceivedProfileCounter() = delete;

  // Gets the counts of all successfully collected profiles, broken down by
  // process type and thread type. "Successfully collected" is defined pretty
  // minimally (we got a couple of frames).
  CallStackProfileMetricsProvider::ProcessThreadCount
  GetSuccessfullyCollectedCounts();

  // Given a list of profiles returned from PendingProfiles::RetrieveProfiles(),
  // add counts from all the successful profiles in the list to our counts for
  // later.
  void OnRetrieveProfiles(const std::vector<SampledProfile>& profiles);

  // Allows testing against the initial state multiple times.
  void ResetToDefaultStateForTesting();  // IN-TEST

 private:
  friend class base::NoDestructor<ReceivedProfileCounter>;

  ReceivedProfileCounter() = default;

  // Returns true if the given profile was success enough to be counted in
  // retrieved_successful_counts_.
  static bool WasMinimallySuccessful(const SampledProfile& profile);

  mutable base::Lock lock_;

  // Count of successfully-stack-walked SampledProfiles retrieved since startup.
  // "success" is defined by WasMinimallySuccessful().
  CallStackProfileMetricsProvider::ProcessThreadCount
      retrieved_successful_counts_ GUARDED_BY(lock_);
};

// static
ReceivedProfileCounter* ReceivedProfileCounter::GetInstance() {
  static base::NoDestructor<ReceivedProfileCounter> instance;
  return instance.get();
}

// static
bool ReceivedProfileCounter::WasMinimallySuccessful(
    const SampledProfile& profile) {
  // If we don't have a process or thread, we don't understand the profile.
  if (!profile.has_process() || !profile.has_thread()) {
    return false;
  }

  // Since we can't symbolize the stacks, "successful" here just means that the
  // stack has at least 2 frames. (The current instruction pointer should always
  // count as one, so two means we had some luck walking the stack.)
  const auto& stacks = profile.call_stack_profile().stack();
  return base::ranges::find_if(stacks,
                               [](const CallStackProfile::Stack& stack) {
                                 return stack.frame_size() >= 2;
                               }) != stacks.end();
}

void ReceivedProfileCounter::OnRetrieveProfiles(
    const std::vector<SampledProfile>& profiles) {
  base::AutoLock scoped_lock(lock_);
  for (const auto& profile : profiles) {
    if (WasMinimallySuccessful(profile)) {
      ++retrieved_successful_counts_[profile.process()][profile.thread()];
    }
  }
}

CallStackProfileMetricsProvider::ProcessThreadCount
ReceivedProfileCounter::GetSuccessfullyCollectedCounts() {
  CallStackProfileMetricsProvider::ProcessThreadCount successful_counts;

  {
    base::AutoLock scoped_lock(lock_);
    // Start with count of profiles we've already sent
    successful_counts = retrieved_successful_counts_;
  }

  // And then add in any pending ones. Copying and then deserializing all the
  // profiles is expensive, but again, this should only be called during tast
  // integration tests.
  for (const SampledProfile& profile :
       PendingProfiles::GetInstance()->GetUnretrievedProfiles()) {
    if (WasMinimallySuccessful(profile)) {
      ++successful_counts[profile.process()][profile.thread()];
    }
  }

  return successful_counts;
}

void ReceivedProfileCounter::ResetToDefaultStateForTesting() {
  base::AutoLock scoped_lock(lock_);
  retrieved_successful_counts_.clear();
}

#endif  // BUILDFLAG(IS_CHROMEOS)
}  // namespace

// CallStackProfileMetricsProvider --------------------------------------------

BASE_FEATURE(kSamplingProfilerReporting,
             "SamplingProfilerReporting",
             kSamplingProfilerReportingDefaultState);

CallStackProfileMetricsProvider::CallStackProfileMetricsProvider() = default;
CallStackProfileMetricsProvider::~CallStackProfileMetricsProvider() = default;

// static
void CallStackProfileMetricsProvider::ReceiveProfile(
    base::TimeTicks profile_start_time,
    SampledProfile profile) {
  if (GetCpuInterceptorCallbackInstance() &&
      (profile.trigger_event() == SampledProfile::PROCESS_STARTUP ||
       profile.trigger_event() == SampledProfile::PERIODIC_COLLECTION)) {
    GetCpuInterceptorCallbackInstance().Run(std::move(profile));
    return;
  }

  if (profile.trigger_event() != SampledProfile::PERIODIC_HEAP_COLLECTION &&
      !SamplingProfilerReportingEnabled()) {
    return;
  }
  PendingProfiles::GetInstance()->MaybeCollectProfile(profile_start_time,
                                                      std::move(profile));
}

// static
void CallStackProfileMetricsProvider::ReceiveSerializedProfile(
    base::TimeTicks profile_start_time,
    bool is_heap_profile,
    mojom::SampledProfilePtr serialized_profile) {
  // Note: All parameters of this function come from a Mojo message from an
  // untrusted process.
  if (GetCpuInterceptorCallbackInstance()) {
    // GetCpuInterceptorCallbackInstance() is set only in tests, so it's safe to
    // trust `is_heap_profile` and `serialized_profile` here.
    DCHECK(!is_heap_profile);
    SampledProfile profile;
    if (base::OptionalUnwrapTo(
            serialized_profile->contents.As<SampledProfile>(), profile)) {
      DCHECK(profile.trigger_event() == SampledProfile::PROCESS_STARTUP ||
             profile.trigger_event() == SampledProfile::PERIODIC_COLLECTION);
      GetCpuInterceptorCallbackInstance().Run(std::move(profile));
    }
    return;
  }

  // If an attacker spoofs `is_heap_profile` or `profile_start_time`, the worst
  // they can do is cause `serialized_profile` to be sent to UMA when profile
  // reporting should be disabled.
  if (!is_heap_profile && !SamplingProfilerReportingEnabled()) {
    return;
  }
  PendingProfiles::GetInstance()->MaybeCollectSerializedProfile(
      profile_start_time, std::move(serialized_profile));
}

// static
void CallStackProfileMetricsProvider::SetCpuInterceptorCallbackForTesting(
    InterceptorCallback callback) {
  GetCpuInterceptorCallbackInstance() = std::move(callback);
}

#if BUILDFLAG(IS_CHROMEOS)
// static
CallStackProfileMetricsProvider::ProcessThreadCount
CallStackProfileMetricsProvider::GetSuccessfullyCollectedCounts() {
  return ReceivedProfileCounter::GetInstance()
      ->GetSuccessfullyCollectedCounts();
}
#endif

void CallStackProfileMetricsProvider::OnRecordingEnabled() {
  PendingProfiles::GetInstance()->SetCollectionEnabled(true);
}

void CallStackProfileMetricsProvider::OnRecordingDisabled() {
  PendingProfiles::GetInstance()->SetCollectionEnabled(false);
}

void CallStackProfileMetricsProvider::ProvideCurrentSessionData(
    ChromeUserMetricsExtension* uma_proto) {
  std::vector<SampledProfile> profiles =
      PendingProfiles::GetInstance()->RetrieveProfiles();
#if BUILDFLAG(IS_CHROMEOS)
  ReceivedProfileCounter::GetInstance()->OnRetrieveProfiles(profiles);
#endif

  for (auto& profile : profiles) {
    // Only heap samples should ever be received if SamplingProfilerReporting is
    // disabled.
    DCHECK(SamplingProfilerReportingEnabled() ||
           profile.trigger_event() == SampledProfile::PERIODIC_HEAP_COLLECTION);
    *uma_proto->add_sampled_profile() = std::move(profile);
  }
}

// static
void CallStackProfileMetricsProvider::ResetStaticStateForTesting() {
  PendingProfiles::GetInstance()->ResetToDefaultStateForTesting();
#if BUILDFLAG(IS_CHROMEOS)
  ReceivedProfileCounter::GetInstance()
      ->ResetToDefaultStateForTesting();  // IN-TEST
#endif
}

}  // namespace metrics
