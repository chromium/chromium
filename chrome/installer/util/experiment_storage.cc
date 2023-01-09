// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/util/experiment_storage.h"

#include <windows.h>

#include <stdint.h>

#include <limits>
#include <string>

#include "base/base64.h"
#include "base/check.h"
#include "base/debug/crash_logging.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/task_traits.h"
#include "base/time/time.h"
#include "base/win/registry.h"
#include "base/win/security_descriptor.h"
#include "base/win/win_util.h"
#include "chrome/install_static/install_details.h"
#include "chrome/install_static/install_modes.h"
#include "chrome/install_static/install_util.h"
#include "chrome/installer/util/experiment.h"
#include "chrome/installer/util/experiment_labels.h"
#include "chrome/installer/util/experiment_metrics.h"
#include "chrome/installer/util/google_update_settings.h"
#include "chrome/installer/util/shell_util.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace installer {

namespace {

constexpr wchar_t kExperimentLabelName[] = L"CrExp60";
constexpr wchar_t kRegKeyRetention[] = L"\\Retention";
constexpr wchar_t kRegValueActionDelay[] = L"ActionDelay";
constexpr wchar_t kRegValueFirstDisplayTime[] = L"FirstDisplayTime";
constexpr wchar_t kRegValueGroup[] = L"Group";
constexpr wchar_t kRegValueInactiveDays[] = L"InactiveDays";
constexpr wchar_t kRegValueLatestDisplayTime[] = L"LatestDisplayTime";
constexpr wchar_t kRegValueRetentionStudy[] = L"RetentionStudy";
constexpr wchar_t kRegValueState[] = L"State";
constexpr wchar_t kRegValueToastCount[] = L"ToastCount";
constexpr wchar_t kRegValueToastLocation[] = L"ToastLocation";
constexpr wchar_t kRegValueUserSessionUptime[] = L"UserSessionUptime";
// Grant Administrators and interactive users full access.
constexpr wchar_t kMutexSecurity[] = L"D:(A;;GA;;;BA)(A;;GA;;;IU)";

constexpr int kSessionLengthBucketLowestBit = 0;
constexpr int kActionDelayBucketLowestBit =
    ExperimentMetrics::kSessionLengthBucketBits + kSessionLengthBucketLowestBit;
constexpr int kLastUsedBucketLowestBit =
    ExperimentMetrics::kActionDelayBucketBits + kActionDelayBucketLowestBit;
constexpr int kToastHourLowestBit =
    ExperimentMetrics::kLastUsedBucketBits + kLastUsedBucketLowestBit;
constexpr int kFirstToastOffsetLowestBit =
    ExperimentMetrics::kToastHourBits + kToastHourLowestBit;
constexpr int kToastCountLowestBit =
    ExperimentMetrics::kFirstToastOffsetBits + kFirstToastOffsetLowestBit;
constexpr int kToastLocationLowestBit =
    ExperimentMetrics::kToastCountBits + kToastCountLowestBit;
constexpr int kStateLowestBit =
    ExperimentMetrics::kToastLocationBits + kToastLocationLowestBit;
constexpr int kGroupLowestBit = ExperimentMetrics::kStateBits + kStateLowestBit;
constexpr int kLowestUnusedBit =
    ExperimentMetrics::kGroupBits + kGroupLowestBit;

// Helper functions ------------------------------------------------------------

// Returns the name of the global mutex used to protect the storage location.
std::wstring GetMutexName() {
  std::wstring name(L"Global\\");
  name.append(install_static::kCompanyPathName);
  name.append(ShellUtil::GetBrowserModelId(!install_static::IsSystemInstall()));
  name.append(L"ExperimentStorageMutex");
  return name;
}

// Populates |path| with the path to the registry key in which the current
// user's experiment state is stored. Returns false if the path cannot be
// determined.
bool GetExperimentStateKeyPath(bool system_level, std::wstring* path) {
  const install_static::InstallDetails& install_details =
      install_static::InstallDetails::Get();

  if (!system_level) {
    *path = install_details.GetClientStateKeyPath().append(kRegKeyRetention);
    return true;
  }

  std::wstring user_sid;
  if (base::win::GetUserSidString(&user_sid)) {
    *path = install_details.GetClientStateMediumKeyPath()
                .append(kRegKeyRetention)
                .append(L"\\")
                .append(user_sid);
    return true;
  }

  NOTREACHED();
  return false;
}

bool OpenParticipationKey(bool write_access, base::win::RegKey* key) {
  const install_static::InstallDetails& details =
      install_static::InstallDetails::Get();
  LONG result = key->Open(
      details.system_level() ? HKEY_LOCAL_MACHINE : HKEY_CURRENT_USER,
      details.GetClientStateKeyPath().c_str(),
      KEY_WOW64_32KEY | (write_access ? KEY_SET_VALUE : KEY_QUERY_VALUE));
  return result == ERROR_SUCCESS;
}

// Reads |value_name| into |result|. Returns false if the value is not found or
// is out of range.
template <class T>
bool ReadBoundedDWORD(base::win::RegKey* key,
                      const wchar_t* value_name,
                      DWORD min_value,
                      DWORD max_value,
                      T* result) {
  DWORD dword_value;
  if (key->ReadValueDW(value_name, &dword_value) != ERROR_SUCCESS)
    return false;
  if (dword_value < min_value || dword_value > max_value)
    return false;
  *result = static_cast<T>(dword_value);
  return true;
}

// Reads the internal representation of a Time or TimeDelta from |value_name|
// into |result|. Returns false if the value is not found or is out of range.
template <class T>
bool ReadTime(base::win::RegKey* key, const wchar_t* value_name, T* result) {
  int64_t qword_value;
  if (key->ReadInt64(value_name, &qword_value) != ERROR_SUCCESS)
    return false;
  *result = T::FromInternalValue(qword_value);
  return true;
}

void WriteTime(base::win::RegKey* key,
               const wchar_t* value_name,
               int64_t internal_time_value) {
  key->WriteValue(value_name, &internal_time_value, sizeof(internal_time_value),
                  REG_QWORD);
}

}  // namespace

// ExperimentStorage::Lock -----------------------------------------------------

ExperimentStorage::Lock::~Lock() {
  BOOL result = ::ReleaseMutex(storage_->mutex_.Get());
  DCHECK(result);
}

bool ExperimentStorage::Lock::ReadParticipation(Study* participation) {
  base::win::RegKey key;
  // A failure to open the key likely indicates that this isn't running from a
  // real install of Chrome.
  if (!OpenParticipationKey(false /* !write_access */, &key))
    return false;

  DWORD value = 0;
  LONG result = key.ReadValueDW(kRegValueRetentionStudy, &value);
  // An error most likely means that the value is not present.
  if (result != ERROR_SUCCESS || value == 0)
    *participation = kNoStudySelected;
  else if (value == 1)
    *participation = kStudyOne;
  else
    *participation = kStudyTwo;
  return true;
}

bool ExperimentStorage::Lock::WriteParticipation(Study participation) {
  DCHECK(participation == kNoStudySelected || participation == kStudyOne ||
         participation == kStudyTwo);
  base::win::RegKey key;
  // A failure to open the key likely indicates that this isn't running from a
  // real install of Chrome.
  if (!OpenParticipationKey(true /* write_access */, &key))
    return false;

  if (participation == kNoStudySelected)
    return key.DeleteValue(kRegValueRetentionStudy) == ERROR_SUCCESS;
  return key.WriteValue(kRegValueRetentionStudy, participation) ==
         ERROR_SUCCESS;
}

bool ExperimentStorage::Lock::LoadExperiment(Experiment* experiment) {
  // This function loads both the experiment metrics and state from the
  // registry.
  // - If no metrics are found: |experiment| is cleared, and true is returned.
  //   (Per-user experiment data in the registry is ignored for all users.)
  // - If metrics indicate an initial state (prior to a user being elected into
  //   an experiment group): |experiment| is populated with the metrics and true
  //   is returned. (Per-user experiment data in the registry is ignored for all
  //   users.)
  // - If metrics indicate an intermediate or terminal state and per-user
  //   experiment data is in the same state: |experiment| is populated with all
  //   data from the registry and true is returned.
  // Otherwise, the metrics correspond to a different user on the machine, so
  // false is returned.

  *experiment = Experiment();

  ExperimentMetrics metrics;
  if (!storage_->LoadMetricsUnsafe(&metrics))
    return false;  // Error reading metrics -- do nothing.

  if (metrics.InInitialState()) {
    // There should be no per-user experiment data present (ignore it if there
    // happens to be somehow).
    experiment->InitializeFromMetrics(metrics);
    return true;
  }

  Experiment temp_experiment;
  if (!storage_->LoadStateUnsafe(&temp_experiment))
    return false;

  // Verify that the state matches the metrics. Ignore the state if this is not
  // the case, as the metrics are the source of truth.
  if (temp_experiment.state() != metrics.state)
    return false;

  *experiment = temp_experiment;
  experiment->metrics_ = metrics;
  return true;
}

bool ExperimentStorage::Lock::StoreExperiment(const Experiment& experiment) {
  bool ret = storage_->StoreMetricsUnsafe(experiment.metrics());
  return storage_->StoreStateUnsafe(experiment) && ret;
}

bool ExperimentStorage::Lock::LoadMetrics(ExperimentMetrics* metrics) {
  DCHECK_EQ(ExperimentMetrics::kUninitialized, metrics->state);
  return storage_->LoadMetricsUnsafe(metrics);
}

bool ExperimentStorage::Lock::StoreMetrics(const ExperimentMetrics& metrics) {
  DCHECK_NE(ExperimentMetrics::kUninitialized, metrics.state);
  return storage_->StoreMetricsUnsafe(metrics);
}

ExperimentStorage::Lock::Lock(ExperimentStorage* storage) : storage_(storage) {
  DCHECK(storage);
  DWORD result = ::WaitForSingleObject(storage_->mutex_.Get(), INFINITE);
  PLOG_IF(FATAL, result == WAIT_FAILED)
      << "Failed to lock ExperimentStorage mutex";
}

// ExperimentStorage -----------------------------------------------------------

ExperimentStorage::ExperimentStorage() {
  // Diagnose failure to create mutex; see https://crbug.com/1351849.
  const auto mutex_name = GetMutexName();
  SCOPED_CRASH_KEY_STRING256("ExperimentStorage", "mutex_name",
                             base::WideToASCII(mutex_name));
  absl::optional<base::win::SecurityDescriptor> sd =
      base::win::SecurityDescriptor::FromSddl(kMutexSecurity);
  PCHECK(sd) << "Failed to create ExperimentStorage mutex security descriptor";
  SECURITY_DESCRIPTOR sd_absolute = {};
  sd->ToAbsolute(sd_absolute);
  SECURITY_ATTRIBUTES attributes = {};
  attributes.nLength = sizeof(attributes);
  attributes.lpSecurityDescriptor = &sd_absolute;
  HANDLE mutex = ::CreateMutex(&attributes, FALSE, mutex_name.c_str());
  PCHECK(mutex) << "Failed to create ExperimentStorage mutex";
  mutex_.Set(mutex);
}

ExperimentStorage::~ExperimentStorage() {}

std::unique_ptr<ExperimentStorage::Lock> ExperimentStorage::AcquireLock() {
  return base::WrapUnique(new Lock(this));
}

// static
int ExperimentStorage::ReadUint64Bits(uint64_t source,
                                      int bit_length,
                                      int low_bit) {
  DCHECK(bit_length > 0 && bit_length <= static_cast<int>(sizeof(int) * 8) &&
         low_bit + bit_length <= static_cast<int>(sizeof(source) * 8));
  uint64_t bit_mask = (1ULL << bit_length) - 1;
  return static_cast<int>((source >> low_bit) & bit_mask);
}

// static
void ExperimentStorage::SetUint64Bits(int value,
                                      int bit_length,
                                      int low_bit,
                                      uint64_t* target) {
  DCHECK(bit_length > 0 && bit_length <= static_cast<int>(sizeof(value) * 8) &&
         low_bit + bit_length <= static_cast<int>(sizeof(*target) * 8));
  uint64_t bit_mask = (1ULL << bit_length) - 1;
  *target |= ((static_cast<uint64_t>(value) & bit_mask) << low_bit);
}

bool ExperimentStorage::DecodeMetrics(base::WStringPiece encoded_metrics,
                                      ExperimentMetrics* metrics) {
  std::string metrics_data;

  if (!base::Base64Decode(base::WideToASCII(encoded_metrics), &metrics_data))
    return false;

  if (metrics_data.size() != 6)
    return false;

  uint64_t metrics_value = 0;
  for (size_t i = 0; i < metrics_data.size(); ++i)
    SetUint64Bits(metrics_data[i], 8, 8 * i, &metrics_value);

  ExperimentMetrics result;
  result.session_length_bucket =
      ReadUint64Bits(metrics_value, ExperimentMetrics::kSessionLengthBucketBits,
                     kSessionLengthBucketLowestBit);
  result.action_delay_bucket =
      ReadUint64Bits(metrics_value, ExperimentMetrics::kActionDelayBucketBits,
                     kActionDelayBucketLowestBit);
  result.last_used_bucket =
      ReadUint64Bits(metrics_value, ExperimentMetrics::kLastUsedBucketBits,
                     kLastUsedBucketLowestBit);
  result.toast_hour = ReadUint64Bits(
      metrics_value, ExperimentMetrics::kToastHourBits, kToastHourLowestBit);
  result.first_toast_offset_days =
      ReadUint64Bits(metrics_value, ExperimentMetrics::kFirstToastOffsetBits,
                     kFirstToastOffsetLowestBit);
  result.toast_count = ReadUint64Bits(
      metrics_value, ExperimentMetrics::kToastCountBits, kToastCountLowestBit);
  result.toast_location = static_cast<ExperimentMetrics::ToastLocation>(
      ReadUint64Bits(metrics_value, ExperimentMetrics::kToastLocationBits,
                     kToastLocationLowestBit));

  static_assert(ExperimentMetrics::State::NUM_STATES <=
                    (1 << ExperimentMetrics::kStateBits),
                "Too many states for ExperimentMetrics encoding.");
  result.state = static_cast<ExperimentMetrics::State>(ReadUint64Bits(
      metrics_value, ExperimentMetrics::kStateBits, kStateLowestBit));
  result.group = ReadUint64Bits(metrics_value, ExperimentMetrics::kGroupBits,
                                kGroupLowestBit);

  if (ReadUint64Bits(metrics_value,
                     sizeof(metrics_value) * 8 - kLowestUnusedBit,
                     kLowestUnusedBit)) {
    return false;
  }

  *metrics = result;
  return true;
}

// static
std::wstring ExperimentStorage::EncodeMetrics(
    const ExperimentMetrics& metrics) {
  uint64_t metrics_value = 0;
  SetUint64Bits(metrics.session_length_bucket,
                ExperimentMetrics::kSessionLengthBucketBits,
                kSessionLengthBucketLowestBit, &metrics_value);
  SetUint64Bits(metrics.action_delay_bucket,
                ExperimentMetrics::kActionDelayBucketBits,
                kActionDelayBucketLowestBit, &metrics_value);
  SetUint64Bits(metrics.last_used_bucket,
                ExperimentMetrics::kLastUsedBucketBits,
                kLastUsedBucketLowestBit, &metrics_value);
  SetUint64Bits(metrics.toast_hour, ExperimentMetrics::kToastHourBits,
                kToastHourLowestBit, &metrics_value);
  SetUint64Bits(metrics.first_toast_offset_days,
                ExperimentMetrics::kFirstToastOffsetBits,
                kFirstToastOffsetLowestBit, &metrics_value);
  SetUint64Bits(metrics.toast_count, ExperimentMetrics::kToastCountBits,
                kToastCountLowestBit, &metrics_value);
  SetUint64Bits(metrics.toast_location, ExperimentMetrics::kToastLocationBits,
                kToastLocationLowestBit, &metrics_value);
  static_assert(ExperimentMetrics::State::NUM_STATES <=
                    (1 << ExperimentMetrics::kStateBits),
                "Too many states for ExperimentMetrics encoding.");
  SetUint64Bits(metrics.state, ExperimentMetrics::kStateBits, kStateLowestBit,
                &metrics_value);
  SetUint64Bits(metrics.group, ExperimentMetrics::kGroupBits, kGroupLowestBit,
                &metrics_value);

  std::string metrics_data(6, '\0');
  for (size_t i = 0; i < metrics_data.size(); ++i) {
    metrics_data[i] =
        static_cast<char>(ReadUint64Bits(metrics_value, 8, 8 * i));
  }
  std::string encoded_metrics;
  base::Base64Encode(metrics_data, &encoded_metrics);
  return base::ASCIIToWide(encoded_metrics);
}

bool ExperimentStorage::LoadMetricsUnsafe(ExperimentMetrics* metrics) {
  std::wstring value;

  if (!GoogleUpdateSettings::ReadExperimentLabels(&value))
    return false;

  ExperimentLabels experiment_labels(value);
  base::WStringPiece encoded_metrics =
      experiment_labels.GetValueForLabel(kExperimentLabelName);
  if (encoded_metrics.empty()) {
    *metrics = ExperimentMetrics();
    return true;
  }

  return DecodeMetrics(encoded_metrics, metrics);
}

bool ExperimentStorage::StoreMetricsUnsafe(const ExperimentMetrics& metrics) {
  std::wstring value;
  if (!GoogleUpdateSettings::ReadExperimentLabels(&value))
    return false;
  ExperimentLabels experiment_labels(value);

  experiment_labels.SetValueForLabel(kExperimentLabelName,
                                     EncodeMetrics(metrics), base::Days(182));

  return GoogleUpdateSettings::SetExperimentLabels(experiment_labels.value());
}

bool ExperimentStorage::LoadStateUnsafe(Experiment* experiment) {
  const bool system_level = install_static::IsSystemInstall();

  std::wstring path;
  if (!GetExperimentStateKeyPath(system_level, &path))
    return false;

  const HKEY root = system_level ? HKEY_LOCAL_MACHINE : HKEY_CURRENT_USER;
  base::win::RegKey key;
  if (key.Open(root, path.c_str(), KEY_QUERY_VALUE | KEY_WOW64_32KEY) !=
      ERROR_SUCCESS) {
    return false;
  }

  return ReadBoundedDWORD(&key, kRegValueState, 0,
                          ExperimentMetrics::NUM_STATES, &experiment->state_) &&
         ReadBoundedDWORD(&key, kRegValueGroup, 0,
                          ExperimentMetrics::kNumGroups - 1,
                          &experiment->group_) &&
         ReadBoundedDWORD(&key, kRegValueToastLocation, 0, 1,
                          &experiment->toast_location_) &&
         ReadBoundedDWORD(&key, kRegValueInactiveDays, 0, INT_MAX,
                          &experiment->inactive_days_) &&
         ReadBoundedDWORD(&key, kRegValueToastCount, 0,
                          ExperimentMetrics::kMaxToastCount,
                          &experiment->toast_count_) &&
         ReadTime(&key, kRegValueFirstDisplayTime,
                  &experiment->first_display_time_) &&
         ReadTime(&key, kRegValueLatestDisplayTime,
                  &experiment->latest_display_time_) &&
         ReadTime(&key, kRegValueUserSessionUptime,
                  &experiment->user_session_uptime_) &&
         ReadTime(&key, kRegValueActionDelay, &experiment->action_delay_);
}

bool ExperimentStorage::StoreStateUnsafe(const Experiment& experiment) {
  const bool system_level = install_static::IsSystemInstall();

  std::wstring path;
  if (!GetExperimentStateKeyPath(system_level, &path))
    return false;

  const HKEY root = system_level ? HKEY_LOCAL_MACHINE : HKEY_CURRENT_USER;
  base::win::RegKey key;
  if (key.Create(root, path.c_str(), KEY_SET_VALUE | KEY_WOW64_32KEY) !=
      ERROR_SUCCESS) {
    return false;
  }

  key.WriteValue(kRegValueState, experiment.state());
  key.WriteValue(kRegValueGroup, experiment.group());
  key.WriteValue(kRegValueToastLocation, experiment.toast_location());
  key.WriteValue(kRegValueInactiveDays, experiment.inactive_days());
  key.WriteValue(kRegValueToastCount, experiment.toast_count());
  WriteTime(&key, kRegValueFirstDisplayTime,
            experiment.first_display_time().ToInternalValue());
  WriteTime(&key, kRegValueLatestDisplayTime,
            experiment.latest_display_time().ToInternalValue());
  WriteTime(&key, kRegValueUserSessionUptime,
            experiment.user_session_uptime().ToInternalValue());
  WriteTime(&key, kRegValueActionDelay,
            experiment.action_delay().ToInternalValue());
  return true;
}

}  // namespace installer
