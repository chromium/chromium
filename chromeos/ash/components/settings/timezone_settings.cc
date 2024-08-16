// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chromeos/ash/components/settings/timezone_settings.h"

#include <stddef.h>

#include <memory>
#include <string>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/i18n/unicodestring.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/memory/singleton.h"
#include "base/observer_list.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/system/sys_info.h"
#include "base/task/task_runner.h"
#include "base/task/thread_pool.h"
#include "chromeos/ash/components/settings/timezone_settings_helper.h"

namespace ash {
namespace system {
const char kUTCTimezoneName[] = "Etc/GMT";
}  // namespace system
}  // namespace ash

namespace {

// The filepath to the timezone file that symlinks to the actual timezone file.
const char kTimezoneSymlink[] = "/var/lib/timezone/localtime";
const char kTimezoneSymlink2[] = "/var/lib/timezone/localtime2";

// The directory that contains all the timezone files. So for timezone
// "US/Pacific", the actual timezone file is: "/usr/share/zoneinfo/US/Pacific"
const char kTimezoneFilesDir[] = "/usr/share/zoneinfo/";

// Fallback time zone ID used in case of an unexpected error.
const char kFallbackTimeZoneId[] = "America/Los_Angeles";

// TODO(jungshik): Using Enumerate method in ICU gives 600+ timezones.
// Even after filtering out duplicate entries with a strict identity check,
// we still have 400+ zones. Relaxing the criteria for the timezone
// identity is likely to cut down the number to < 100. Until we
// come up with a better list, we hard-code the following list. It came from
// from Android initially, but more entries have been added.
// Note that the list is sorted in terms of timezone offset from UTC.
static const char* kTimeZones[] = {
    "Pacific/Midway",
    "Pacific/Honolulu",
    "America/Anchorage",
    "America/Los_Angeles",
    "America/Vancouver",
    "America/Tijuana",
    "America/Phoenix",
    "America/Chihuahua",
    "America/Denver",
    "America/Edmonton",
    "America/Mazatlan",
    "America/Regina",
    "America/Costa_Rica",
    "America/Chicago",
    "America/Mexico_City",
    "America/Tegucigalpa",
    "America/Winnipeg",
    "Pacific/Easter",
    "America/Bogota",
    "America/Lima",
    "America/New_York",
    "America/Toronto",
    "America/Caracas",
    "America/Barbados",
    "America/Halifax",
    "America/Manaus",
    "America/Santiago",
    "America/St_Johns",
    "America/Araguaina",
    "America/Argentina/Buenos_Aires",
    "America/Argentina/San_Luis",
    "America/Montevideo",
    "America/Santiago",
    "America/Sao_Paulo",
    "America/Godthab",
    "Atlantic/South_Georgia",
    "Atlantic/Cape_Verde",
    ash::system::kUTCTimezoneName,
    "Atlantic/Azores",
    "Atlantic/Reykjavik",
    "Atlantic/St_Helena",
    "Africa/Casablanca",
    "Atlantic/Faroe",
    "Europe/Dublin",
    "Europe/Lisbon",
    "Europe/London",
    "Europe/Amsterdam",
    "Europe/Belgrade",
    "Europe/Berlin",
    "Europe/Bratislava",
    "Europe/Brussels",
    "Europe/Budapest",
    "Europe/Copenhagen",
    "Europe/Ljubljana",
    "Europe/Madrid",
    "Europe/Malta",
    "Europe/Oslo",
    "Europe/Paris",
    "Europe/Prague",
    "Europe/Rome",
    "Europe/Stockholm",
    "Europe/Sarajevo",
    "Europe/Tirane",
    "Europe/Vaduz",
    "Europe/Vienna",
    "Europe/Warsaw",
    "Europe/Zagreb",
    "Europe/Zurich",
    "Africa/Windhoek",
    "Africa/Lagos",
    "Africa/Brazzaville",
    "Africa/Cairo",
    "Africa/Harare",
    "Africa/Maputo",
    "Africa/Johannesburg",
    "Europe/Kaliningrad",
    "Europe/Athens",
    "Europe/Bucharest",
    "Europe/Chisinau",
    "Europe/Helsinki",
    "Europe/Istanbul",
    "Europe/Kiev",
    "Europe/Riga",
    "Europe/Sofia",
    "Europe/Tallinn",
    "Europe/Vilnius",
    "Asia/Amman",
    "Asia/Beirut",
    "Asia/Jerusalem",
    "Africa/Nairobi",
    "Asia/Baghdad",
    "Asia/Riyadh",
    "Asia/Kuwait",
    "Europe/Minsk",
    "Europe/Moscow",
    "Asia/Tehran",
    "Europe/Samara",
    "Asia/Dubai",
    "Asia/Tbilisi",
    "Indian/Mauritius",
    "Asia/Baku",
    "Asia/Yerevan",
    "Asia/Kabul",
    "Asia/Karachi",
    "Asia/Aqtobe",
    "Asia/Ashgabat",
    "Asia/Oral",
    "Asia/Yekaterinburg",
    "Asia/Calcutta",
    "Asia/Colombo",
    "Asia/Katmandu",
    "Asia/Omsk",
    "Asia/Almaty",
    "Asia/Dhaka",
    "Asia/Novosibirsk",
    "Asia/Rangoon",
    "Asia/Bangkok",
    "Asia/Jakarta",
    "Asia/Krasnoyarsk",
    "Asia/Novokuznetsk",
    "Asia/Ho_Chi_Minh",
    "Asia/Phnom_Penh",
    "Asia/Vientiane",
    "Asia/Shanghai",
    "Asia/Hong_Kong",
    "Asia/Kuala_Lumpur",
    "Asia/Singapore",
    "Asia/Manila",
    "Asia/Taipei",
    "Asia/Ulaanbaatar",
    "Asia/Makassar",
    "Asia/Irkutsk",
    "Asia/Yakutsk",
    "Australia/Perth",
    "Australia/Eucla",
    "Asia/Seoul",
    "Asia/Tokyo",
    "Asia/Jayapura",
    "Asia/Sakhalin",
    "Asia/Vladivostok",
    "Asia/Magadan",
    "Australia/Darwin",
    "Australia/Adelaide",
    "Pacific/Guam",
    "Australia/Brisbane",
    "Australia/Hobart",
    "Australia/Sydney",
    "Asia/Anadyr",
    "Pacific/Port_Moresby",
    "Asia/Kamchatka",
    "Pacific/Fiji",
    "Pacific/Majuro",
    "Pacific/Auckland",
    "Pacific/Tongatapu",
    "Pacific/Apia",
    "Pacific/Kiritimati",
};

std::string GetTimezoneIDAsString() {
  // Compare with chromiumos/src/platform/init/ui.conf which fixes certain
  // incorrect states of the timezone symlink on startup. Thus errors occuring
  // here should be rather contrived.

  // Look at kTimezoneSymlink, see which timezone we are symlinked to.
  char buf[256];
  const ssize_t len = readlink(kTimezoneSymlink, buf, sizeof(buf) - 1);
  if (len == -1) {
    LOG(ERROR) << "GetTimezoneID: Cannot read timezone symlink "
               << kTimezoneSymlink;
    return std::string();
  }

  std::string timezone(buf, len);
  // Remove kTimezoneFilesDir from the beginning.
  if (!base::StartsWith(timezone, kTimezoneFilesDir,
                        base::CompareCase::SENSITIVE)) {
    LOG(ERROR) << "GetTimezoneID: Timezone symlink is wrong " << timezone;
    return std::string();
  }

  return timezone.substr(strlen(kTimezoneFilesDir));
}

void SetTimezoneIDFromString(const std::string& id) {
  // Change the kTimezoneSymlink symlink to the path for this timezone.
  // We want to do this in an atomic way. So we are going to create the symlink
  // at kTimezoneSymlink2 and then move it to kTimezoneSymlink

  base::FilePath timezone_symlink(kTimezoneSymlink);
  base::FilePath timezone_symlink2(kTimezoneSymlink2);
  base::FilePath timezone_file(kTimezoneFilesDir + id);

  // Make sure timezone_file exists.
  if (!base::PathExists(timezone_file)) {
    LOG(ERROR) << "SetTimezoneID: Cannot find timezone file "
               << timezone_file.value();
    return;
  }

  // Delete old symlink2 if it exists.
  base::DeleteFile(timezone_symlink2);

  // Create new symlink2.
  if (symlink(timezone_file.value().c_str(),
              timezone_symlink2.value().c_str()) == -1) {
    LOG(ERROR) << "SetTimezoneID: Unable to create symlink "
               << timezone_symlink2.value() << " to " << timezone_file.value();
    return;
  }

  // Move symlink2 to symlink.
  if (!base::ReplaceFile(timezone_symlink2, timezone_symlink, NULL)) {
    LOG(ERROR) << "SetTimezoneID: Unable to move symlink "
               << timezone_symlink2.value() << " to "
               << timezone_symlink.value();
  }
}

// Common code of the TimezoneSettings implementations.
class TimezoneSettingsBaseImpl : public ash::system::TimezoneSettings {
 public:
  TimezoneSettingsBaseImpl(const TimezoneSettingsBaseImpl&) = delete;
  TimezoneSettingsBaseImpl& operator=(const TimezoneSettingsBaseImpl&) = delete;

  ~TimezoneSettingsBaseImpl() override;

  // TimezoneSettings implementation:
  const icu::TimeZone& GetTimezone() override;
  std::u16string GetCurrentTimezoneID() override;
  void SetTimezoneFromID(const std::u16string& timezone_id) override;
  void AddObserver(Observer* observer) override;
  void RemoveObserver(Observer* observer) override;
  const std::vector<std::unique_ptr<icu::TimeZone>>& GetTimezoneList()
      const override;

 protected:
  TimezoneSettingsBaseImpl();

  // Returns |timezone| if it is an element of |timezones_|.
  // Otherwise, returns a timezone from |timezones_|, if such exists, that has
  // the same rule as the given |timezone|.
  // Otherwise, returns NULL.
  // Note multiple timezones with the same time zone rules may exist
  // e.g.
  //   US/Pacific == America/Los_Angeles
  const icu::TimeZone* GetKnownTimezoneOrNull(
      const icu::TimeZone& timezone) const;

  base::ObserverList<Observer>::Unchecked observers_;
  std::vector<std::unique_ptr<icu::TimeZone>> timezones_;
  std::unique_ptr<icu::TimeZone> timezone_;
};

// The TimezoneSettings implementation used in production.
class TimezoneSettingsImpl : public TimezoneSettingsBaseImpl {
 public:
  // TimezoneSettings implementation:
  void SetTimezone(const icu::TimeZone& timezone) override;

  static TimezoneSettingsImpl* GetInstance();

  TimezoneSettingsImpl(const TimezoneSettingsImpl&) = delete;
  TimezoneSettingsImpl& operator=(const TimezoneSettingsImpl&) = delete;

 private:
  friend struct base::DefaultSingletonTraits<TimezoneSettingsImpl>;

  TimezoneSettingsImpl();
};

// The stub TimezoneSettings implementation used on Linux desktop.
class TimezoneSettingsStubImpl : public TimezoneSettingsBaseImpl {
 public:
  // TimezoneSettings implementation:
  void SetTimezone(const icu::TimeZone& timezone) override;

  static TimezoneSettingsStubImpl* GetInstance();

  TimezoneSettingsStubImpl(const TimezoneSettingsStubImpl&) = delete;
  TimezoneSettingsStubImpl& operator=(const TimezoneSettingsStubImpl&) = delete;

 private:
  friend struct base::DefaultSingletonTraits<TimezoneSettingsStubImpl>;

  TimezoneSettingsStubImpl();
};

TimezoneSettingsBaseImpl::~TimezoneSettingsBaseImpl() = default;

const icu::TimeZone& TimezoneSettingsBaseImpl::GetTimezone() {
  return *timezone_.get();
}

std::u16string TimezoneSettingsBaseImpl::GetCurrentTimezoneID() {
  return ash::system::TimezoneSettings::GetTimezoneID(GetTimezone());
}

void TimezoneSettingsBaseImpl::SetTimezoneFromID(
    const std::u16string& timezone_id) {
  std::unique_ptr<icu::TimeZone> timezone(icu::TimeZone::createTimeZone(
      icu::UnicodeString(timezone_id.c_str(), timezone_id.size())));
  SetTimezone(*timezone);
}

void TimezoneSettingsBaseImpl::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void TimezoneSettingsBaseImpl::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

const std::vector<std::unique_ptr<icu::TimeZone>>&
TimezoneSettingsBaseImpl::GetTimezoneList() const {
  return timezones_;
}

TimezoneSettingsBaseImpl::TimezoneSettingsBaseImpl() {
  for (size_t i = 0; i < std::size(kTimeZones); ++i) {
    timezones_.push_back(base::WrapUnique(icu::TimeZone::createTimeZone(
        icu::UnicodeString(kTimeZones[i], -1, US_INV))));
  }
}

const icu::TimeZone* TimezoneSettingsBaseImpl::GetKnownTimezoneOrNull(
    const icu::TimeZone& timezone) const {
  return ash::system::GetKnownTimezoneOrNull(timezone, timezones_);
}

void TimezoneSettingsImpl::SetTimezone(const icu::TimeZone& timezone) {
  // Replace |timezone| by a known timezone with the same rules. If none exists
  // go on with |timezone|.
  const icu::TimeZone* known_timezone = GetKnownTimezoneOrNull(timezone);
  if (!known_timezone)
    known_timezone = &timezone;

  timezone_.reset(known_timezone->clone());
  std::string id = base::UTF16ToUTF8(GetTimezoneID(*known_timezone));
  VLOG(1) << "Setting timezone to " << id;
  // It's safe to change the timezone config files in the background as the
  // following operations don't depend on the completion of the config change.
  base::ThreadPool::PostTask(FROM_HERE,
                             {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
                              base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
                             base::BindOnce(&SetTimezoneIDFromString, id));
  icu::TimeZone::setDefault(*known_timezone);
  for (auto& observer : observers_)
    observer.TimezoneChanged(*known_timezone);
}

// static
TimezoneSettingsImpl* TimezoneSettingsImpl::GetInstance() {
  return base::Singleton<
      TimezoneSettingsImpl,
      base::DefaultSingletonTraits<TimezoneSettingsImpl>>::get();
}

TimezoneSettingsImpl::TimezoneSettingsImpl() {
  std::string id = GetTimezoneIDAsString();
  if (id.empty()) {
    id = kFallbackTimeZoneId;
    LOG(ERROR) << "Got an empty string for timezone, default to '" << id;
  }

  timezone_.reset(
      icu::TimeZone::createTimeZone(icu::UnicodeString::fromUTF8(id)));

  // Store a known timezone equivalent to id in |timezone_|.
  const icu::TimeZone* known_timezone = GetKnownTimezoneOrNull(*timezone_);
  if (known_timezone != NULL && *known_timezone != *timezone_)
    // Not necessary to update the filesystem because |known_timezone| has the
    // same rules.
    timezone_.reset(known_timezone->clone());

  icu::TimeZone::setDefault(*timezone_);
  VLOG(1) << "Timezone initially set to " << id;
  icu::UnicodeString resolvedId;
  std::string resolvedIdStr;
  timezone_->getID(resolvedId);
  VLOG(1) << "Timezone initially resolved to "
          << resolvedId.toUTF8String(resolvedIdStr);
}

void TimezoneSettingsStubImpl::SetTimezone(const icu::TimeZone& timezone) {
  // Replace |timezone| by a known timezone with the same rules. If none exists
  // go on with |timezone|.
  const icu::TimeZone* known_timezone = GetKnownTimezoneOrNull(timezone);
  if (!known_timezone)
    known_timezone = &timezone;

  std::string id = base::UTF16ToUTF8(GetTimezoneID(*known_timezone));
  VLOG(1) << "Setting timezone to " << id;
  timezone_.reset(known_timezone->clone());
  icu::TimeZone::setDefault(*known_timezone);
  for (auto& observer : observers_)
    observer.TimezoneChanged(*known_timezone);
}

// static
TimezoneSettingsStubImpl* TimezoneSettingsStubImpl::GetInstance() {
  return base::Singleton<
      TimezoneSettingsStubImpl,
      base::DefaultSingletonTraits<TimezoneSettingsStubImpl>>::get();
}

TimezoneSettingsStubImpl::TimezoneSettingsStubImpl() {
  timezone_.reset(icu::TimeZone::createDefault());
  const icu::TimeZone* known_timezone = GetKnownTimezoneOrNull(*timezone_);
  if (known_timezone != NULL && *known_timezone != *timezone_)
    timezone_.reset(known_timezone->clone());
}

}  // namespace

namespace ash {
namespace system {

TimezoneSettings::Observer::~Observer() = default;

// static
TimezoneSettings* TimezoneSettings::GetInstance() {
  if (base::SysInfo::IsRunningOnChromeOS()) {
    return TimezoneSettingsImpl::GetInstance();
  } else {
    return TimezoneSettingsStubImpl::GetInstance();
  }
}

// static
std::u16string TimezoneSettings::GetTimezoneID(const icu::TimeZone& timezone) {
  icu::UnicodeString id;
  return base::i18n::UnicodeStringToString16(timezone.getID(id));
}

}  // namespace system
}  // namespace ash
