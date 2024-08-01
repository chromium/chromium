// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/browser/upgrade_detector/upgrade_detector_impl.h"

#include <stdint.h>

#include <optional>
#include <string>

#include "base/build_time.h"
#include "base/check_op.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/no_destructor.h"
#include "base/notreached.h"
#include "base/ranges/algorithm.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/clock.h"
#include "base/time/default_clock.h"
#include "base/time/default_tick_clock.h"
#include "base/time/tick_clock.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/buildflags.h"
#include "chrome/browser/enterprise/browser_management/management_service_factory.h"
#include "chrome/browser/google/google_brand.h"
#include "chrome/browser/obsolete_system/obsolete_system.h"
#include "chrome/browser/upgrade_detector/build_state.h"
#include "chrome/browser/upgrade_detector/get_installed_version.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "components/network_time/network_time_tracker.h"
#include "components/prefs/pref_service.h"
#include "components/version_info/version_info.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"

#if BUILDFLAG(IS_WIN)
#include "chrome/installer/util/google_update_settings.h"
#endif

namespace {

// The default thresholds for reaching annoyance levels.
constexpr auto kDefaultVeryLowThreshold = base::Hours(1);
constexpr auto kDefaultLowThreshold = base::Days(2);
constexpr auto kDefaultElevatedThreshold = base::Days(4);
constexpr auto kDefaultHighThreshold = base::Days(7);
constexpr auto kDefaultGraceThreshold = kDefaultHighThreshold - base::Hours(1);

// How long to wait (each cycle) before checking which severity level we should
// be at. Once we reach the highest severity, the timer will stop.
constexpr auto kNotifyCycleTime = base::Minutes(20);

// Same as kNotifyCycleTimeMs but only used during testing.
constexpr auto kNotifyCycleTimeForTesting = base::Milliseconds(500);

// How often to check to see if the build has become outdated.
constexpr auto kOutdatedBuildDetectorPeriod = base::Days(1);

// The number of days after which we identify a build/install as outdated.
constexpr auto kOutdatedBuildAge = base::Days(7) * 8;

bool ShouldDetectOutdatedBuilds() {
#if BUILDFLAG(ENABLE_UPDATE_NOTIFICATIONS) && !BUILDFLAG(IS_CHROMEOS)
  // Don't show the bubble if we have a brand code that is NOT organic
  std::string brand;
  if (google_brand::GetBrand(&brand) && !google_brand::IsOrganic(brand)) {
    return false;
  }

  // Don't show the bubble for Enterprise users.
  if (policy::ManagementServiceFactory::GetForPlatform()->IsManaged()) {
    return false;
  }

  return true;
#else
  // Outdated build detection is not relevant on ChromeOS platforms where
  // updates are handled differently than on other desktop platforms.
  return false;
#endif
}

// Check if one of the outdated simulation switches was present on the command
// line.
bool SimulatingOutdated() {
  const base::CommandLine& cmd_line = *base::CommandLine::ForCurrentProcess();
  return cmd_line.HasSwitch(switches::kSimulateOutdated) ||
         cmd_line.HasSwitch(switches::kSimulateOutdatedNoAU);
}

// Check if any of the testing switches was present on the command line.
bool IsTesting() {
  const base::CommandLine& cmd_line = *base::CommandLine::ForCurrentProcess();
  return cmd_line.HasSwitch(switches::kSimulateUpgrade) ||
         cmd_line.HasSwitch(switches::kCheckForUpdateIntervalSec) ||
         cmd_line.HasSwitch(switches::kSimulateCriticalUpdate) ||
         SimulatingOutdated();
}

}  // namespace

UpgradeDetectorImpl::UpgradeDetectorImpl(const base::Clock* clock,
                                         const base::TickClock* tick_clock)
    : UpgradeDetector(clock, tick_clock),
      outdated_build_timer_(this->tick_clock()),
      upgrade_notification_timer_(this->tick_clock()),
      is_auto_update_enabled_(true),
      simulating_outdated_(SimulatingOutdated()),
      is_testing_(simulating_outdated_ || IsTesting()),
      build_date_(base::GetBuildTime()) {}

UpgradeDetectorImpl::~UpgradeDetectorImpl() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void UpgradeDetectorImpl::StartUpgradeNotificationTimer() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // The timer may already be running (e.g. due to both a software upgrade and
  // experiment updates being available).
  if (upgrade_notification_timer_.IsRunning())
    return;

  if (upgrade_detected_time().is_null())
    set_upgrade_detected_time(clock()->Now());

  // Compute the thresholds for the annoyance levels.
  CalculateThresholds();

  // Broadcast the appropriate notification.
  NotifyOnUpgrade();
}

void UpgradeDetectorImpl::CalculateThresholds() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!upgrade_detected_time().is_null());

  DoCalculateThresholds();

#if DCHECK_IS_ON()
  // |stages_| must be sorted in decreasing order of time.
  for (std::array<base::TimeDelta, kNumStages>::iterator scan =
           stages_.begin() + 1;
       scan != stages_.end(); ++scan) {
    DCHECK_GE(*(scan - 1), *scan);
  }
#endif  // DCHECK_IS_ON()
}

void UpgradeDetectorImpl::DoCalculateThresholds() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  base::TimeDelta notification_period = GetRelaunchNotificationPeriod();
  const std::optional<RelaunchWindow> relaunch_window =
      GetRelaunchWindowPolicyValue();

  if (notification_period.is_zero() && !relaunch_window) {
    // Use the default values when no override is set and we don't expect to
    // adjust the levels according to the relaunch time interval.
    stages_[kStagesIndexHigh] = kDefaultHighThreshold;
    stages_[kStagesIndexGrace] = kDefaultGraceThreshold;
    stages_[kStagesIndexElevated] = kDefaultElevatedThreshold;
    stages_[kStagesIndexLow] = kDefaultLowThreshold;
    stages_[kStagesIndexVeryLow] = kDefaultVeryLowThreshold;
  } else {
    // Calculate the "high" level using the notification period and adjust it to
    // fall within the relaunch time interval. The adjusted "high" level is
    // divided evenly to set the 'low' and 'elevated' levels.
    base::TimeDelta effective_notification_period = notification_period;
    if (notification_period.is_zero())
      effective_notification_period = kDefaultHighThreshold;

    const RelaunchWindow effective_relaunch_window =
        relaunch_window.value_or(GetDefaultRelaunchWindow());

    DCHECK(!upgrade_detected_time().is_null());
    const base::Time adjusted_deadline =
        AdjustDeadline(upgrade_detected_time() + effective_notification_period,
                       effective_relaunch_window);
    effective_notification_period = adjusted_deadline - upgrade_detected_time();

    stages_[kStagesIndexHigh] = effective_notification_period;
    stages_[kStagesIndexLow] = effective_notification_period / 3;
    stages_[kStagesIndexElevated] =
        effective_notification_period - stages_[kStagesIndexLow];
    base::TimeDelta grace_period = GetGracePeriod(
        stages_[kStagesIndexHigh] - stages_[kStagesIndexElevated]);
    stages_[kStagesIndexGrace] = stages_[kStagesIndexHigh] - grace_period;
    // "Very low" is one hour, unless "low" is even less.
    stages_[kStagesIndexVeryLow] =
        std::min(stages_[kStagesIndexLow], kDefaultVeryLowThreshold);
  }

  // When testing, scale everything back so that a day passes in ten seconds.
  if (is_testing_ && !relaunch_window) {
    constexpr int64_t scale_factor = base::Days(1) / base::Seconds(10);
    for (auto& stage : stages_)
      stage /= scale_factor;
  }
}

void UpgradeDetectorImpl::StartOutdatedBuildDetector() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  static BASE_FEATURE(kOutdatedBuildDetector, "OutdatedBuildDetector",
                      base::FEATURE_ENABLED_BY_DEFAULT);

  if (!base::FeatureList::IsEnabled(kOutdatedBuildDetector))
    return;

  // Don't detect outdated builds for obsolete operating systems when new builds
  // are no longer available.
  if (ObsoleteSystem::IsObsoleteNowOrSoon() &&
      ObsoleteSystem::IsEndOfTheLine()) {
    return;
  }

  // Don't show the bubble for certain conditions unless an outdated build is
  // being simulated by command line switches.
  if (!simulating_outdated_) {
    if (!ShouldDetectOutdatedBuilds())
      return;

#if BUILDFLAG(IS_WIN)
    // Only check to if autoupdates are enabled if the user has not already been
    // asked about re-enabling them.
    if (!g_browser_process->local_state() ||
        !g_browser_process->local_state()->GetBoolean(
            prefs::kAttemptedToEnableAutoupdate)) {
      is_auto_update_enabled_ = GoogleUpdateSettings::AreAutoupdatesEnabled();
    }
#endif
  }

  DetectOutdatedInstall();
}

void UpgradeDetectorImpl::DetectOutdatedInstall() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  base::Time current_time;
  base::TimeDelta uncertainty;
  bool is_network_time = true;
  if (g_browser_process->network_time_tracker()->GetNetworkTime(&current_time,
                                                                &uncertainty) !=
      network_time::NetworkTimeTracker::NETWORK_TIME_AVAILABLE) {
    // When network time has not been initialized yet, simply rely on the
    // machine's current time.
    is_network_time = false;
    current_time = base::Time::Now();
  }

  CHECK(!current_time.is_null());
  CHECK(!build_date_.is_null());

  if (!simulating_outdated_ && is_network_time && build_date_ > current_time) {
    // Sometimes unexpected things happen with clocks; ignore these edge cases.
    // See https://crbug.com/40062693 for related discussions.
    return;
  }

  if (current_time - build_date_ > kOutdatedBuildAge) {
    UpgradeDetected(is_auto_update_enabled_
                        ? UPGRADE_NEEDED_OUTDATED_INSTALL
                        : UPGRADE_NEEDED_OUTDATED_INSTALL_NO_AU);
  } else {
    outdated_build_timer_.Start(
        FROM_HERE, kOutdatedBuildDetectorPeriod,
        base::BindOnce(&UpgradeDetectorImpl::DetectOutdatedInstall,
                       base::Unretained(this)));
  }
}

void UpgradeDetectorImpl::UpgradeDetected(UpgradeAvailable upgrade_available) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  set_upgrade_available(upgrade_available);
  set_critical_update_acknowledged(false);

  if (upgrade_available != UPGRADE_AVAILABLE_NONE ||
      critical_experiment_updates_available()) {
    StartUpgradeNotificationTimer();
  } else {
    // There is no longer anything to notify the user about, so stop the timer
    // and reset state.
    upgrade_notification_timer_.Stop();
    set_upgrade_detected_time(base::Time());
    set_upgrade_notification_stage(UPGRADE_ANNOYANCE_NONE);
    stages_.fill(base::TimeDelta());
  }
}

void UpgradeDetectorImpl::OnExperimentChangesDetected(Severity severity) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  set_best_effort_experiment_updates_available(severity == BEST_EFFORT);
  set_critical_experiment_updates_available(severity == CRITICAL);
  StartUpgradeNotificationTimer();
}

void UpgradeDetectorImpl::NotifyOnUpgradeWithTimePassed(
    base::TimeDelta time_passed) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  const UpgradeNotificationAnnoyanceLevel last_stage =
      upgrade_notification_stage();

  // Figure out which stage the detector is now in (new_stage) and how far away
  // the next highest stage is (next_delay).
  UpgradeNotificationAnnoyanceLevel new_stage = UPGRADE_ANNOYANCE_NONE;
  base::TimeDelta next_delay;

  if (upgrade_available() > UPGRADE_AVAILABLE_REGULAR ||
      critical_experiment_updates_available()) {
    new_stage = UPGRADE_ANNOYANCE_CRITICAL;
  } else {
    // |stages_| must be sorted by decreasing TimeDelta.
    std::array<base::TimeDelta, kNumStages>::iterator it =
        base::ranges::lower_bound(stages_, time_passed,
                                  base::ranges::greater());
    if (it != stages_.end())
      new_stage = StageIndexToAnnoyanceLevel(it - stages_.begin());
    if (it != stages_.begin())
      next_delay = *(it - 1) - time_passed;
  }

  set_upgrade_notification_stage(new_stage);
  if (!next_delay.is_zero()) {
    // Schedule the next wakeup in 20 minutes or when the next change to the
    // notification stage should take place.
    upgrade_notification_timer_.Start(
        FROM_HERE,
        std::min(next_delay,
                 is_testing_ ? kNotifyCycleTimeForTesting : kNotifyCycleTime),
        this, &UpgradeDetectorImpl::NotifyOnUpgrade);
  } else if (upgrade_notification_timer_.IsRunning()) {
    // Explicitly stop the timer in case this call is due to a change (e.g., in
    // the RelaunchNotificationPeriod) that brought the instance up to or above
    // the "high" annoyance level.
    upgrade_notification_timer_.Stop();
  }

  // Issue a notification if the stage is above "none" or if it's dropped down
  // to "none" from something higher.
  if (new_stage != UPGRADE_ANNOYANCE_NONE ||
      last_stage != UPGRADE_ANNOYANCE_NONE) {
    NotifyUpgrade();
  }
}

base::TimeDelta UpgradeDetectorImpl::GetThresholdForLevel(
    UpgradeNotificationAnnoyanceLevel level) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!stages_[0].is_zero());
  return stages_[AnnoyanceLevelToStagesIndex(level)];
}

// static
UpgradeDetectorImpl::LevelIndex
UpgradeDetectorImpl::AnnoyanceLevelToStagesIndex(
    UpgradeNotificationAnnoyanceLevel level) {
  switch (level) {
    case UPGRADE_ANNOYANCE_NONE:
      break;  // Invalid input.
    case UPGRADE_ANNOYANCE_VERY_LOW:
      return kStagesIndexVeryLow;
    case UPGRADE_ANNOYANCE_LOW:
      return kStagesIndexLow;
    case UPGRADE_ANNOYANCE_ELEVATED:
      return kStagesIndexElevated;
    case UPGRADE_ANNOYANCE_GRACE:
      return kStagesIndexGrace;
    case UPGRADE_ANNOYANCE_HIGH:
      break;
    case UPGRADE_ANNOYANCE_CRITICAL:
      break;  // Invalid input.
  }
  DCHECK_EQ(level, UPGRADE_ANNOYANCE_HIGH);
  return kStagesIndexHigh;
}

// static
UpgradeDetector::UpgradeNotificationAnnoyanceLevel
UpgradeDetectorImpl::StageIndexToAnnoyanceLevel(size_t index) {
  static constexpr UpgradeNotificationAnnoyanceLevel kIndexToLevel[] = {
      UpgradeDetector::UPGRADE_ANNOYANCE_HIGH,
      UpgradeDetector::UPGRADE_ANNOYANCE_GRACE,
      UpgradeDetector::UPGRADE_ANNOYANCE_ELEVATED,
      UpgradeDetector::UPGRADE_ANNOYANCE_LOW,
      UpgradeDetector::UPGRADE_ANNOYANCE_VERY_LOW};
  static_assert(std::size(kIndexToLevel) == kNumStages, "mismatch");
  DCHECK_LT(index, std::size(kIndexToLevel));
  return kIndexToLevel[index];
}

void UpgradeDetectorImpl::OnMonitoredPrefsChanged() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Broadcast the appropriate notification if an upgrade has been detected.
  if (upgrade_available() != UPGRADE_AVAILABLE_NONE) {
    // Force a recomputation of the thresholds.
    CalculateThresholds();
    NotifyOnUpgrade();
  }
}

void UpgradeDetectorImpl::NotifyOnUpgrade() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  const base::TimeDelta time_passed = clock()->Now() - upgrade_detected_time();
  NotifyOnUpgradeWithTimePassed(time_passed);
}

// static
UpgradeDetectorImpl* UpgradeDetectorImpl::GetInstance() {
  static base::NoDestructor<UpgradeDetectorImpl> instance(
      base::DefaultClock::GetInstance(), base::DefaultTickClock::GetInstance());
  return instance.get();
}

void UpgradeDetectorImpl::Init() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  UpgradeDetector::Init();

  const base::CommandLine& cmd_line = *base::CommandLine::ForCurrentProcess();
  // The different command line switches that affect testing can't be used
  // simultaneously, if they do, here's the precedence order, based on the order
  // of the if statements below:
  // - kDisableBackgroundNetworking prevents any of the other command line
  //   switch from being taken into account.
  // - kSimulateOutdatedNoAU has precedence over kSimulateOutdated.
  // - kSimulateOutdated[NoAu] can work on its own, or with a specified date.
  if (cmd_line.HasSwitch(switches::kDisableBackgroundNetworking))
    return;

  if (simulating_outdated_) {
    // The outdated simulation can work without a value, which means outdated
    // now, or with a value that must be a well formed date/time string that
    // overrides the build date.
    // Also note that to test with a given time/date, until the network time
    // tracking moves off of the VariationsService, the "variations-server-url"
    // command line switch must also be specified for the service to be
    // available on non GOOGLE_CHROME_BRANDING.
    std::string switch_name;
    if (cmd_line.HasSwitch(switches::kSimulateOutdatedNoAU)) {
      is_auto_update_enabled_ = false;
      switch_name = switches::kSimulateOutdatedNoAU;
    } else {
      switch_name = switches::kSimulateOutdated;
    }
    std::string build_date = cmd_line.GetSwitchValueASCII(switch_name);
    base::Time maybe_build_time;
    bool result = base::Time::FromString(build_date.c_str(), &maybe_build_time);
    if (result && !maybe_build_time.is_null()) {
      // We got a valid build date simulation so use it and check for upgrades.
      build_date_ = maybe_build_time;
    } else {
      // Without a valid date, we simulate that we are already outdated...
      UpgradeDetected(is_auto_update_enabled_
                          ? UPGRADE_NEEDED_OUTDATED_INSTALL
                          : UPGRADE_NEEDED_OUTDATED_INSTALL_NO_AU);
      return;
    }
  }

  // Register for experiment notifications.
  variations::VariationsService* variations_service =
      g_browser_process->variations_service();
  if (variations_service) {
    variations_service->AddObserver(this);
  }

#if BUILDFLAG(ENABLE_UPDATE_NOTIFICATIONS)
  // Start checking for outdated builds sometime after startup completes.
  content::GetUIThreadTaskRunner({base::TaskPriority::BEST_EFFORT})
      ->PostTask(
          FROM_HERE,
          base::BindOnce(&UpgradeDetectorImpl::StartOutdatedBuildDetector,
                         weak_factory_.GetWeakPtr()));

  auto* const build_state = g_browser_process->GetBuildState();
  build_state->AddObserver(this);
  installed_version_poller_.emplace(build_state);
#endif  // BUILDFLAG(ENABLE_UPDATE_NOTIFICATIONS)
}

void UpgradeDetectorImpl::Shutdown() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  weak_factory_.InvalidateWeakPtrs();
  variations::VariationsService* variations_service =
      g_browser_process->variations_service();
  if (variations_service)
    variations_service->RemoveObserver(this);
  installed_version_poller_.reset();
  g_browser_process->GetBuildState()->RemoveObserver(this);
  outdated_build_timer_.Stop();
  stages_.fill(base::TimeDelta());

  UpgradeDetector::Shutdown();
}

base::Time UpgradeDetectorImpl::GetAnnoyanceLevelDeadline(
    UpgradeNotificationAnnoyanceLevel level) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  const base::Time detected_time = upgrade_detected_time();
  if (detected_time.is_null())
    return detected_time;
  switch (level) {
    case UpgradeDetector::UPGRADE_ANNOYANCE_NONE:
      return detected_time;
    case UpgradeDetector::UPGRADE_ANNOYANCE_VERY_LOW:
    case UpgradeDetector::UPGRADE_ANNOYANCE_LOW:
    case UpgradeDetector::UPGRADE_ANNOYANCE_ELEVATED:
    case UpgradeDetector::UPGRADE_ANNOYANCE_GRACE:
    case UpgradeDetector::UPGRADE_ANNOYANCE_HIGH:
      return detected_time + GetThresholdForLevel(level);
    case UpgradeDetector::UPGRADE_ANNOYANCE_CRITICAL:
      return upgrade_notification_stage() == UPGRADE_ANNOYANCE_CRITICAL
                 ? detected_time
                 : base::Time();
  }
}

void UpgradeDetectorImpl::OnUpdate(const BuildState* build_state) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (build_state->update_type() == BuildState::UpdateType::kNone) {
    // An update was available, but seemingly no longer is. Perhaps an update
    // was followed by a rollback. Back off if nothing more important was
    // previously noticed (e.g., a critical experiment config change or an
    // outdated build).
    if (upgrade_available() == UPGRADE_AVAILABLE_REGULAR ||
        upgrade_available() == UPGRADE_AVAILABLE_CRITICAL) {
      UpgradeDetected(UPGRADE_AVAILABLE_NONE);
    }
  } else {
    // build_state->installed_version() will not have a value in case of an
    // error fetching the installed version. This is generally an indication
    // that something has gone wrong, so behave as if a normal update is
    // available in the hopes that a restart will make everything alright.
    UpgradeDetected(build_state->critical_version() > version_info::GetVersion()
                        ? UPGRADE_AVAILABLE_CRITICAL
                        : UPGRADE_AVAILABLE_REGULAR);
  }
}

// static
UpgradeDetector* UpgradeDetector::GetInstance() {
  return UpgradeDetectorImpl::GetInstance();
}

// static
base::TimeDelta UpgradeDetector::GetDefaultHighAnnoyanceThreshold() {
  return kDefaultHighThreshold;
}

// static
base::TimeDelta UpgradeDetector::GetDefaultElevatedAnnoyanceThreshold() {
  return kDefaultElevatedThreshold;
}

// static
UpgradeDetector::RelaunchWindow UpgradeDetector::GetDefaultRelaunchWindow() {
  // Relaunch window is the whole day and any time is within the window.
  return RelaunchWindow(/*start_hour=*/0, /*start_minute=*/0, base::Hours(24));
}
