// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/metrics_log.h"

#include <stddef.h>

#include <algorithm>
#include <cstring>
#include <string>
#include <string_view>
#include <vector>

#include "base/build_time.h"
#include "base/command_line.h"
#include "base/cpu.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram_base.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/histogram_samples.h"
#include "base/metrics/histogram_snapshot_manager.h"
#include "base/metrics/metrics_hashes.h"
#include "base/strings/string_util.h"
#include "base/system/sys_info.h"
#include "base/time/clock.h"
#include "base/time/default_clock.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "components/flags_ui/flags_ui_switches.h"
#include "components/metrics/delegating_provider.h"
#include "components/metrics/environment_recorder.h"
#include "components/metrics/histogram_encoder.h"
#include "components/metrics/metrics_pref_names.h"
#include "components/metrics/metrics_provider.h"
#include "components/metrics/metrics_service_client.h"
#include "components/network_time/network_time_tracker.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/variations/hashing.h"
#include "crypto/random.h"
#include "third_party/icu/source/i18n/unicode/timezone.h"
#include "third_party/metrics_proto/histogram_event.pb.h"
#include "third_party/metrics_proto/system_profile.pb.h"
#include "third_party/metrics_proto/user_action_event.pb.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/build_info.h"
#endif

#if BUILDFLAG(IS_WIN)
#include <windows.h>

#include "base/win/current_module.h"
#endif

#if BUILDFLAG(IS_LINUX)
#include "base/environment.h"
#include "base/nix/xdg_util.h"
#endif

using base::SampleCountIterator;

namespace metrics {

LogMetadata::LogMetadata()
    : samples_count(std::nullopt), user_id(std::nullopt) {}
LogMetadata::LogMetadata(
    const std::optional<base::HistogramBase::Count> samples_count,
    const std::optional<uint64_t> user_id,
    const std::optional<metrics::UkmLogSourceType> log_source_type)
    : samples_count(samples_count),
      user_id(user_id),
      log_source_type(log_source_type) {}
LogMetadata::LogMetadata(const LogMetadata& other) = default;
LogMetadata::~LogMetadata() = default;

void LogMetadata::AddSampleCount(base::HistogramBase::Count sample_count) {
  if (samples_count.has_value()) {
    samples_count = samples_count.value() + sample_count;
  } else {
    samples_count = sample_count;
  }
}

namespace {

// Convenience function to return the given time at a resolution in seconds.
static int64_t ToMonotonicSeconds(base::TimeTicks time_ticks) {
  return (time_ticks - base::TimeTicks()).InSeconds();
}

// Helper function to get, increment, update and return an integer value stored
// in |local_state| using |key|. This helper is used to manage the log record id
// and the finalized log record id.
int IncrementAndUpdate(PrefService* local_state, const std::string& key) {
  const int value = local_state->GetInteger(key) + 1;
  local_state->SetInteger(key, value);
  return value;
}

// Populates |time| with information about the current time and, if
// |record_time_zone| is true, the time zone.
void RecordCurrentTime(
    const base::Clock* clock,
    const network_time::NetworkTimeTracker* network_time_tracker,
    bool record_time_zone,
    metrics::ChromeUserMetricsExtension::RealLocalTime* time) {
  // Record the current time and the clock used to determine the time.
  base::Time now;
  if (network_time_tracker != nullptr &&
      network_time_tracker->GetNetworkTime(&now, nullptr) ==
          network_time::NetworkTimeTracker::NETWORK_TIME_AVAILABLE) {
    // |network_time_tracker| can be null in certain settings such as WebView
    // (which doesn't run a NetworkTimeTracker) and tests.
    time->set_time_source(
        metrics::ChromeUserMetricsExtension::RealLocalTime::NETWORK_TIME_CLOCK);
  } else {
    now = clock->Now();
    time->set_time_source(
        metrics::ChromeUserMetricsExtension::RealLocalTime::CLIENT_CLOCK);
  }
  time->set_time_sec(now.ToTimeT());

  if (record_time_zone) {
    // Determine time zone offset from GMT and store it.
    int32_t raw_offset, dst_offset;
    UErrorCode status = U_ZERO_ERROR;
    // Ask for a new time zone object each time; don't cache it, as time zones
    // may change while Chrome is running.
    std::unique_ptr<icu::TimeZone> time_zone(icu::TimeZone::createDefault());
    time_zone->getOffset(now.InMillisecondsFSinceUnixEpoch(),
                         false,  // interpret |now| as from UTC/GMT
                         raw_offset, dst_offset, status);
    base::TimeDelta time_zone_offset =
        base::Milliseconds(raw_offset + dst_offset);
    if (U_FAILURE(status)) {
      DVLOG(1) << "Failed to get time zone offset, error code: " << status;
      // The fallback case is to get the raw timezone offset ignoring the
      // daylight saving time.
      time_zone_offset = base::Milliseconds(time_zone->getRawOffset());
    }
    time->set_time_zone_offset_from_gmt_sec(time_zone_offset.InSeconds());
  }
}

#if BUILDFLAG(IS_LINUX)
metrics::SystemProfileProto::OS::XdgSessionType ToProtoSessionType(
    base::nix::SessionType session_type) {
  switch (session_type) {
    case base::nix::SessionType::kUnset:
      return metrics::SystemProfileProto::OS::UNSET;
    case base::nix::SessionType::kOther:
      return metrics::SystemProfileProto::OS::OTHER_SESSION_TYPE;
    case base::nix::SessionType::kUnspecified:
      return metrics::SystemProfileProto::OS::UNSPECIFIED;
    case base::nix::SessionType::kTty:
      return metrics::SystemProfileProto::OS::TTY;
    case base::nix::SessionType::kX11:
      return metrics::SystemProfileProto::OS::X11;
    case base::nix::SessionType::kWayland:
      return metrics::SystemProfileProto::OS::WAYLAND;
    case base::nix::SessionType::kMir:
      return metrics::SystemProfileProto::OS::MIR;
  }

  NOTREACHED_IN_MIGRATION();
  return metrics::SystemProfileProto::OS::UNSET;
}

metrics::SystemProfileProto::OS::XdgCurrentDesktop ToProtoCurrentDesktop(
    base::nix::DesktopEnvironment desktop_environment) {
  switch (desktop_environment) {
    case base::nix::DesktopEnvironment::DESKTOP_ENVIRONMENT_OTHER:
      return metrics::SystemProfileProto::OS::OTHER;
    case base::nix::DesktopEnvironment::DESKTOP_ENVIRONMENT_CINNAMON:
      return metrics::SystemProfileProto::OS::CINNAMON;
    case base::nix::DesktopEnvironment::DESKTOP_ENVIRONMENT_DEEPIN:
      return metrics::SystemProfileProto::OS::DEEPIN;
    case base::nix::DesktopEnvironment::DESKTOP_ENVIRONMENT_GNOME:
      return metrics::SystemProfileProto::OS::GNOME;
    case base::nix::DesktopEnvironment::DESKTOP_ENVIRONMENT_KDE3:
    case base::nix::DesktopEnvironment::DESKTOP_ENVIRONMENT_KDE4:
    case base::nix::DesktopEnvironment::DESKTOP_ENVIRONMENT_KDE5:
    case base::nix::DesktopEnvironment::DESKTOP_ENVIRONMENT_KDE6:
      return metrics::SystemProfileProto::OS::KDE;
    case base::nix::DesktopEnvironment::DESKTOP_ENVIRONMENT_PANTHEON:
      return metrics::SystemProfileProto::OS::PANTHEON;
    case base::nix::DesktopEnvironment::DESKTOP_ENVIRONMENT_UKUI:
      return metrics::SystemProfileProto::OS::UKUI;
    case base::nix::DesktopEnvironment::DESKTOP_ENVIRONMENT_UNITY:
      return metrics::SystemProfileProto::OS::UNITY;
    case base::nix::DesktopEnvironment::DESKTOP_ENVIRONMENT_XFCE:
      return metrics::SystemProfileProto::OS::XFCE;
    case base::nix::DesktopEnvironment::DESKTOP_ENVIRONMENT_LXQT:
      return metrics::SystemProfileProto::OS::LXQT;
  }

  NOTREACHED_IN_MIGRATION();
  return metrics::SystemProfileProto::OS::OTHER;
}
#endif  // BUILDFLAG(IS_LINUX)

// Gets the hash of this session. A random hash is generated the first time this
// is called (which is cached and returned for the remainder of the session).
uint64_t GetSessionHash() {
  static const std::vector<uint8_t> session_hash =
      crypto::RandBytesAsVector(/*length=*/8);
  return *reinterpret_cast<const uint64_t*>(session_hash.data());
}

}  // namespace

namespace internal {

SystemProfileProto::InstallerPackage ToInstallerPackage(
    std::string_view installer_package_name) {
  if (installer_package_name.empty())
    return SystemProfileProto::INSTALLER_PACKAGE_NONE;
  if (installer_package_name == "com.android.vending")
    return SystemProfileProto::INSTALLER_PACKAGE_GOOGLE_PLAY_STORE;
  return SystemProfileProto::INSTALLER_PACKAGE_OTHER;
}

}  // namespace internal

MetricsLog::MetricsLog(const std::string& client_id,
                       int session_id,
                       LogType log_type,
                       MetricsServiceClient* client)
    : MetricsLog(client_id,
                 session_id,
                 log_type,
                 base::DefaultClock::GetInstance(),
                 client->GetNetworkTimeTracker(),
                 client) {}

MetricsLog::MetricsLog(const std::string& client_id,
                       int session_id,
                       LogType log_type,
                       base::Clock* clock,
                       const network_time::NetworkTimeTracker* network_clock,
                       MetricsServiceClient* client)
    : closed_(false),
      log_type_(log_type),
      client_(client),
      creation_time_(base::TimeTicks::Now()),
      has_environment_(false),
      clock_(clock),
      network_clock_(network_clock) {
  uma_proto_.set_client_id(Hash(client_id));
  uma_proto_.set_session_id(session_id);

  if (log_type == MetricsLog::ONGOING_LOG) {
    // Don't record the time when creating a log because creating a log happens
    // on startups and setting the timezone requires ICU initialization that is
    // too expensive to run during this critical time.
    RecordCurrentTime(clock_, network_clock_,
                      /*record_time_zone=*/false,
                      uma_proto_.mutable_time_log_created());
  }

  const int32_t product = client_->GetProduct();
  // Only set the product if it differs from the default value.
  if (product != uma_proto_.product())
    uma_proto_.set_product(product);

  SystemProfileProto* system_profile = uma_proto()->mutable_system_profile();
  // Record the unhashed the client_id to system profile. This is used to
  // simulate field trial assignments for the client.
  DCHECK_EQ(client_id.size(), 36ull);
  system_profile->set_client_uuid(client_id);
  RecordCoreSystemProfile(client_, system_profile);
}

MetricsLog::~MetricsLog() = default;

// static
void MetricsLog::RegisterPrefs(PrefRegistrySimple* registry) {
  EnvironmentRecorder::RegisterPrefs(registry);
  registry->RegisterIntegerPref(prefs::kMetricsLogFinalizedRecordId, 0);
  registry->RegisterIntegerPref(prefs::kMetricsLogRecordId, 0);
}

// static
uint64_t MetricsLog::Hash(const std::string& value) {
  uint64_t hash = base::HashMetricName(value);

  // The following log is VERY helpful when folks add some named histogram into
  // the code, but forgot to update the descriptive list of histograms.  When
  // that happens, all we get to see (server side) is a hash of the histogram
  // name.  We can then use this logging to find out what histogram name was
  // being hashed to a given MD5 value by just running the version of Chromium
  // in question with --enable-logging.
  DVLOG(1) << "Metrics: Hash numeric [" << value << "]=[" << hash << "]";

  return hash;
}

// static
int64_t MetricsLog::GetBuildTime() {
  static int64_t integral_build_time = 0;
  if (!integral_build_time)
    integral_build_time = static_cast<int64_t>(base::GetBuildTime().ToTimeT());
  return integral_build_time;
}

// static
int64_t MetricsLog::GetCurrentTime() {
  return ToMonotonicSeconds(base::TimeTicks::Now());
}

void MetricsLog::AssignFinalizedRecordId(PrefService* local_state) {
  DCHECK(!uma_proto_.has_finalized_record_id());
  uma_proto_.set_finalized_record_id(
      IncrementAndUpdate(local_state, prefs::kMetricsLogFinalizedRecordId));
}

void MetricsLog::AssignRecordId(PrefService* local_state) {
  DCHECK(!uma_proto_.has_record_id());
  uma_proto_.set_record_id(
      IncrementAndUpdate(local_state, prefs::kMetricsLogRecordId));
}

void MetricsLog::RecordUserAction(const std::string& key,
                                  base::TimeTicks action_time) {
  DCHECK(!closed_);

  UserActionEventProto* user_action = uma_proto_.add_user_action_event();
  user_action->set_name_hash(Hash(key));
  user_action->set_time_sec(ToMonotonicSeconds(action_time));
  UMA_HISTOGRAM_BOOLEAN("UMA.UserActionsCount", true);
}

// static
void MetricsLog::RecordCoreSystemProfile(MetricsServiceClient* client,
                                         SystemProfileProto* system_profile) {
  RecordCoreSystemProfile(
      client->GetVersionString(), client->GetChannel(),
      client->IsExtendedStableChannel(), client->GetApplicationLocale(),
      client->GetAppPackageNameIfLoggable(), system_profile);

  std::string brand_code;
  if (client->GetBrand(&brand_code))
    system_profile->set_brand_code(brand_code);

  // Records 32-bit hashes of the command line keys.
  base::CommandLine command_line_copy(*base::CommandLine::ForCurrentProcess());

  // Exclude these switches which are very frequently on the command line but
  // serve no meaningful purpose.
  static const char* const kSwitchesToFilter[] = {
      switches::kFlagSwitchesBegin,
      switches::kFlagSwitchesEnd,
  };

  for (const char* filter_switch : kSwitchesToFilter)
    command_line_copy.RemoveSwitch(filter_switch);

  for (const auto& command_line_switch : command_line_copy.GetSwitches()) {
    system_profile->add_command_line_key_hash(
        variations::HashName(command_line_switch.first));
  }
}

// static
void MetricsLog::RecordCoreSystemProfile(
    const std::string& version,
    metrics::SystemProfileProto::Channel channel,
    bool is_extended_stable_channel,
    const std::string& application_locale,
    const std::string& package_name,
    SystemProfileProto* system_profile) {
  system_profile->set_build_timestamp(metrics::MetricsLog::GetBuildTime());
  system_profile->set_app_version(version);
  system_profile->set_channel(channel);
  if (is_extended_stable_channel)
    system_profile->set_is_extended_stable_channel(true);
  system_profile->set_application_locale(application_locale);

#if defined(ADDRESS_SANITIZER) || DCHECK_IS_ON()
  // Set if a build is instrumented (e.g. built with ASAN, or with DCHECKs).
  system_profile->set_is_instrumented_build(true);
#endif

  system_profile->set_session_hash(GetSessionHash());

  metrics::SystemProfileProto::Hardware* hardware =
      system_profile->mutable_hardware();
  hardware->set_cpu_architecture(base::SysInfo::OperatingSystemArchitecture());
  auto app_os_arch = base::SysInfo::ProcessCPUArchitecture();
  if (!app_os_arch.empty())
    hardware->set_app_cpu_architecture(app_os_arch);
  hardware->set_system_ram_mb(base::SysInfo::AmountOfPhysicalMemoryMB());
  hardware->set_hardware_class(base::SysInfo::HardwareModelName());
#if BUILDFLAG(IS_WIN)
  hardware->set_dll_base(reinterpret_cast<uint64_t>(CURRENT_MODULE()));
#endif

  metrics::SystemProfileProto::OS* os = system_profile->mutable_os();
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  // The Lacros browser runs on Chrome OS, but reports a special OS name to
  // differentiate itself from the built-in ash browser + window manager binary.
  os->set_name("Lacros");
#elif BUILDFLAG(IS_CHROMEOS_ASH)
  os->set_name("CrOS");
#else
  os->set_name(base::SysInfo::OperatingSystemName());
#endif
  os->set_version(base::SysInfo::OperatingSystemVersion());

// On ChromeOS, KernelVersion refers to the Linux kernel version and
// OperatingSystemVersion refers to the ChromeOS release version.
#if BUILDFLAG(IS_CHROMEOS_ASH)
  os->set_kernel_version(base::SysInfo::KernelVersion());
#elif BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS_LACROS)
  // Linux operating system version is copied over into kernel version to be
  // consistent.
  os->set_kernel_version(base::SysInfo::OperatingSystemVersion());
#endif

#if BUILDFLAG(IS_ANDROID)
  const auto* build_info = base::android::BuildInfo::GetInstance();
  os->set_build_fingerprint(build_info->android_build_fp());
  if (!package_name.empty() && package_name != "com.android.chrome")
    system_profile->set_app_package_name(package_name);
  system_profile->set_installer_package(
      internal::ToInstallerPackage(build_info->installer_package_name()));
#elif BUILDFLAG(IS_IOS)
  os->set_build_number(base::SysInfo::GetIOSBuildNumber());
#endif

#if BUILDFLAG(IS_LINUX)
  std::unique_ptr<base::Environment> env = base::Environment::Create();
  os->set_xdg_session_type(ToProtoSessionType(base::nix::GetSessionType(*env)));
  os->set_xdg_current_desktop(
      ToProtoCurrentDesktop(base::nix::GetDesktopEnvironment(env.get())));
#endif
}

void MetricsLog::RecordHistogramDelta(const std::string& histogram_name,
                                      const base::HistogramSamples& snapshot) {
  DCHECK(!closed_);
  log_metadata_.AddSampleCount(snapshot.TotalCount());
  EncodeHistogramDelta(histogram_name, snapshot, &uma_proto_);
}

void MetricsLog::RecordPreviousSessionData(
    DelegatingProvider* delegating_provider,
    PrefService* local_state) {
  delegating_provider->ProvidePreviousSessionData(uma_proto());
  // Schedule a Local State write to flush updated prefs to disk. This is done
  // because a side effect of providing data—namely stability data—is updating
  // Local State prefs.
  local_state->CommitPendingWrite();
}

void MetricsLog::RecordCurrentSessionData(
    base::TimeDelta incremental_uptime,
    base::TimeDelta uptime,
    DelegatingProvider* delegating_provider,
    PrefService* local_state) {
  DCHECK(!closed_);
  DCHECK(has_environment_);

  // Record recent delta for critical stability metrics. We can't wait for a
  // restart to gather these, as that delay biases our observation away from
  // users that run happily for a looooong time.  We send increments with each
  // UMA log upload, just as we send histogram data.
  WriteRealtimeStabilityAttributes(incremental_uptime, uptime);

  delegating_provider->ProvideCurrentSessionData(uma_proto());
  // Schedule a Local State write to flush updated prefs to disk. This is done
  // because a side effect of providing data—namely stability data—is updating
  // Local State prefs.
  local_state->CommitPendingWrite();
}

void MetricsLog::WriteMetricsEnableDefault(EnableMetricsDefault metrics_default,
                                           SystemProfileProto* system_profile) {
  if (client_->IsReportingPolicyManaged()) {
    // If it's managed, then it must be reporting, otherwise we wouldn't be
    // sending metrics.
    system_profile->set_uma_default_state(
        SystemProfileProto_UmaDefaultState_POLICY_FORCED_ENABLED);
    return;
  }

  switch (metrics_default) {
    case EnableMetricsDefault::DEFAULT_UNKNOWN:
      // Don't set the field if it's unknown.
      break;
    case EnableMetricsDefault::OPT_IN:
      system_profile->set_uma_default_state(
          SystemProfileProto_UmaDefaultState_OPT_IN);
      break;
    case EnableMetricsDefault::OPT_OUT:
      system_profile->set_uma_default_state(
          SystemProfileProto_UmaDefaultState_OPT_OUT);
  }
}

void MetricsLog::WriteRealtimeStabilityAttributes(
    base::TimeDelta incremental_uptime,
    base::TimeDelta uptime) {
  // Update the stats which are critical for real-time stability monitoring.
  // Since these are "optional," only list ones that are non-zero, as the counts
  // are aggregated (summed) server side.

  SystemProfileProto::Stability* stability =
      uma_proto()->mutable_system_profile()->mutable_stability();

  const uint64_t incremental_uptime_sec = incremental_uptime.InSeconds();
  if (incremental_uptime_sec)
    stability->set_incremental_uptime_sec(incremental_uptime_sec);
  const uint64_t uptime_sec = uptime.InSeconds();
  if (uptime_sec)
    stability->set_uptime_sec(uptime_sec);
}

const SystemProfileProto& MetricsLog::RecordEnvironment(
    DelegatingProvider* delegating_provider) {
  // If |has_environment_| is true, then the system profile in |uma_proto_| has
  // previously been fully filled in. We still want to fill it in again with
  // more up to date information (e.g. current field trials), but in order to
  // not have duplicate repeated fields, we must first clear it. Clearing it
  // will reset the information filled in by RecordCoreSystemProfile() that was
  // previously done in the constructor, so re-add that too.
  //
  // The |has_environment| case will happen on the very first log, where we
  // call RecordEnvironment() in order to persist the system profile in the
  // persistent histograms .pma file.
  if (has_environment_) {
    std::string client_uuid = uma_proto_.system_profile().client_uuid();
    uma_proto_.clear_system_profile();
    MetricsLog::RecordCoreSystemProfile(client_,
                                        uma_proto_.mutable_system_profile());
    uma_proto_.mutable_system_profile()->set_client_uuid(client_uuid);
  }

  has_environment_ = true;

  SystemProfileProto* system_profile = uma_proto_.mutable_system_profile();
  WriteMetricsEnableDefault(client_->GetMetricsReportingDefaultState(),
                            system_profile);

  delegating_provider->ProvideSystemProfileMetricsWithLogCreationTime(
      creation_time_, system_profile);

  return *system_profile;
}

bool MetricsLog::LoadSavedEnvironmentFromPrefs(PrefService* local_state) {
  DCHECK(!has_environment_);
  has_environment_ = true;

  SystemProfileProto* system_profile = uma_proto()->mutable_system_profile();
  EnvironmentRecorder recorder(local_state);
  return recorder.LoadEnvironmentFromPrefs(system_profile);
}

metrics::ChromeUserMetricsExtension::RealLocalTime
MetricsLog::GetCurrentClockTime(bool record_time_zone) {
  CHECK_EQ(log_type_, MetricsLog::ONGOING_LOG);
  metrics::ChromeUserMetricsExtension::RealLocalTime time;
  RecordCurrentTime(clock_, network_clock_, record_time_zone, &time);
  return time;
}

void MetricsLog::FinalizeLog(
    bool truncate_events,
    const std::string& current_app_version,
    std::optional<ChromeUserMetricsExtension::RealLocalTime> close_time,
    std::string* encoded_log) {
  if (truncate_events)
    TruncateEvents();
  RecordLogWrittenByAppVersionIfNeeded(current_app_version);
  if (close_time.has_value()) {
    *uma_proto_.mutable_time_log_closed() = std::move(close_time.value());
  }
  CloseLog();

  uma_proto_.SerializeToString(encoded_log);
}

void MetricsLog::CloseLog() {
  DCHECK(!closed_);

  // Ongoing logs (and only ongoing logs) should have a closed timestamp. Other
  // types of logs (initial stability and independent) contain metrics from
  // previous sessions, so do not add timestamps as they would not accurately
  // represent the time at which those metrics were emitted.
  CHECK(log_type_ == MetricsLog::ONGOING_LOG
            ? uma_proto_.has_time_log_closed()
            : !uma_proto_.has_time_log_closed());

  closed_ = true;
}

void MetricsLog::RecordLogWrittenByAppVersionIfNeeded(
    const std::string& current_version) {
  DCHECK(!closed_);
  if (uma_proto()->system_profile().app_version() != current_version) {
    uma_proto()->mutable_system_profile()->set_log_written_by_app_version(
        current_version);
  }
}

void MetricsLog::TruncateEvents() {
  DCHECK(!closed_);
  if (uma_proto_.user_action_event_size() > internal::kUserActionEventLimit) {
    UMA_HISTOGRAM_COUNTS_100000("UMA.TruncatedEvents.UserAction",
                                uma_proto_.user_action_event_size());
    for (int i = internal::kUserActionEventLimit;
         i < uma_proto_.user_action_event_size(); ++i) {
      // No histograms.xml entry is added for this histogram because it uses an
      // enum that is generated from actions.xml in our processing pipelines.
      // Instead, a histogram description will also be produced in our
      // pipelines.
      UMA_HISTOGRAM_SPARSE(
          "UMA.TruncatedEvents.UserAction.Type",
          // Truncate the unsigned 64-bit hash to 31 bits, to make it a suitable
          // histogram sample.
          uma_proto_.user_action_event(i).name_hash() & 0x7fffffff);
    }
    uma_proto_.mutable_user_action_event()->DeleteSubrange(
        internal::kUserActionEventLimit,
        uma_proto_.user_action_event_size() - internal::kUserActionEventLimit);
  }

  if (uma_proto_.omnibox_event_size() > internal::kOmniboxEventLimit) {
    uma_proto_.mutable_omnibox_event()->DeleteSubrange(
        internal::kOmniboxEventLimit,
        uma_proto_.omnibox_event_size() - internal::kOmniboxEventLimit);
  }
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
void MetricsLog::SetUserId(const std::string& user_id) {
  uint64_t hashed_user_id = Hash(user_id);
  uma_proto_.set_user_id(hashed_user_id);
  log_metadata_.user_id = hashed_user_id;
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

}  // namespace metrics
