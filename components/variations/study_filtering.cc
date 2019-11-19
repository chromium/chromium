// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/variations/study_filtering.h"

#include <stddef.h>
#include <stdint.h>

#include <set>

#include "base/stl_util.h"
#include "base/strings/string_util.h"
#include "components/variations/client_filterable_state.h"

namespace variations {
namespace {

// Converts |date_time| in Study date format to base::Time.
base::Time ConvertStudyDateToBaseTime(int64_t date_time) {
  return base::Time::UnixEpoch() + base::TimeDelta::FromSeconds(date_time);
}

// Similar to base::Contains(), but specifically for ASCII strings and
// case-insensitive comparison.
template <typename Collection>
bool ContainsStringIgnoreCaseASCII(const Collection& collection,
                                   const std::string& value) {
  return std::find_if(std::begin(collection), std::end(collection),
                      [&value](const std::string& s) -> bool {
                        return base::EqualsCaseInsensitiveASCII(s, value);
                      }) != std::end(collection);
}

}  // namespace

namespace internal {

bool CheckStudyChannel(const Study::Filter& filter, Study::Channel channel) {
  // An empty channel list matches all channels.
  if (filter.channel_size() == 0)
    return true;

  for (int i = 0; i < filter.channel_size(); ++i) {
    if (filter.channel(i) == channel)
      return true;
  }
  return false;
}

bool CheckStudyFormFactor(const Study::Filter& filter,
                          Study::FormFactor form_factor) {
  // Empty whitelist and blacklist signifies matching any form factor.
  if (filter.form_factor_size() == 0 && filter.exclude_form_factor_size() == 0)
    return true;

  // Allow the form_factor if it matches the whitelist.
  // Note if both a whitelist and blacklist are specified, the blacklist is
  // ignored. We do not expect both to be present for Chrome due to server-side
  // checks.
  if (filter.form_factor_size() > 0)
    return base::Contains(filter.form_factor(), form_factor);

  // Omit if we match the blacklist.
  return !base::Contains(filter.exclude_form_factor(), form_factor);
}

bool CheckStudyHardwareClass(const Study::Filter& filter,
                             const std::string& hardware_class) {
  // Empty hardware_class and exclude_hardware_class matches all.
  if (filter.hardware_class_size() == 0 &&
      filter.exclude_hardware_class_size() == 0) {
    return true;
  }

  // Note: This logic changed in M66. Prior to M66, this used substring
  // comparison logic to match hardware classes. In M66, it was made consistent
  // with other filters.

  // Checks if we are supposed to filter for a specified set of
  // hardware_classes. Note that this means this overrides the
  // exclude_hardware_class in case that ever occurs (which it shouldn't).
  if (filter.hardware_class_size() > 0) {
    return ContainsStringIgnoreCaseASCII(filter.hardware_class(),
                                         hardware_class);
  }

  // Omit if we match the blacklist.
  return !ContainsStringIgnoreCaseASCII(filter.exclude_hardware_class(),
                                        hardware_class);
}

bool CheckStudyLocale(const Study::Filter& filter, const std::string& locale) {
  // Empty locale and exclude_locale lists matches all locales.
  if (filter.locale_size() == 0 && filter.exclude_locale_size() == 0)
    return true;

  // Check if we are supposed to filter for a specified set of countries. Note
  // that this means this overrides the exclude_locale in case that ever occurs
  // (which it shouldn't).
  if (filter.locale_size() > 0)
    return base::Contains(filter.locale(), locale);

  // Omit if matches any of the exclude entries.
  return !base::Contains(filter.exclude_locale(), locale);
}

bool CheckStudyPlatform(const Study::Filter& filter, Study::Platform platform) {
  for (int i = 0; i < filter.platform_size(); ++i) {
    if (filter.platform(i) == platform)
      return true;
  }
  return false;
}

bool CheckStudyLowEndDevice(const Study::Filter& filter,
                            bool is_low_end_device) {
  return !filter.has_is_low_end_device() ||
         filter.is_low_end_device() == is_low_end_device;
}

bool CheckStudyEnterprise(const Study::Filter& filter,
                          const ClientFilterableState& client_state) {
  return !filter.has_is_enterprise() ||
         filter.is_enterprise() == client_state.IsEnterprise();
}

bool CheckStudyStartDate(const Study::Filter& filter,
                         const base::Time& date_time) {
  if (filter.has_start_date()) {
    const base::Time start_date =
        ConvertStudyDateToBaseTime(filter.start_date());
    return date_time >= start_date;
  }

  return true;
}

bool CheckStudyEndDate(const Study::Filter& filter,
                       const base::Time& date_time) {
  if (filter.has_end_date()) {
    const base::Time end_date = ConvertStudyDateToBaseTime(filter.end_date());
    return end_date >= date_time;
  }

  return true;
}

bool CheckStudyVersion(const Study::Filter& filter,
                       const base::Version& version) {
  if (filter.has_min_version()) {
    if (version.CompareToWildcardString(filter.min_version()) < 0)
      return false;
  }

  if (filter.has_max_version()) {
    if (version.CompareToWildcardString(filter.max_version()) > 0)
      return false;
  }

  return true;
}

bool CheckStudyOSVersion(const Study::Filter& filter,
                         const base::Version& version) {
  if (filter.has_min_os_version()) {
    if (!version.IsValid() ||
        version.CompareToWildcardString(filter.min_os_version()) < 0) {
      return false;
    }
  }

  if (filter.has_max_os_version()) {
    if (!version.IsValid() ||
        version.CompareToWildcardString(filter.max_os_version()) > 0) {
      return false;
    }
  }

  return true;
}

bool CheckStudyCountry(const Study::Filter& filter,
                       const std::string& country) {
  // Empty country and exclude_country matches all.
  if (filter.country_size() == 0 && filter.exclude_country_size() == 0)
    return true;

  // Checks if we are supposed to filter for a specified set of countries. Note
  // that this means this overrides the exclude_country in case that ever occurs
  // (which it shouldn't).
  if (filter.country_size() > 0)
    return base::Contains(filter.country(), country);

  // Omit if matches any of the exclude entries.
  return !base::Contains(filter.exclude_country(), country);
}

const std::string& GetClientCountryForStudy(
    const Study& study,
    const ClientFilterableState& client_state) {
  switch (study.consistency()) {
    case Study::SESSION:
      return client_state.session_consistency_country;
    case Study::PERMANENT:
      // Use the saved country for permanent consistency studies. This allows
      // Chrome to use the same country for filtering permanent consistency
      // studies between Chrome upgrades. Since some studies have user-visible
      // effects, this helps to avoid annoying users with experimental group
      // churn while traveling.
      return client_state.permanent_consistency_country;
  }

  // Unless otherwise specified, use an empty country that won't pass any
  // filters that specifically include countries, but will pass any filters
  // that specifically exclude countries.
  return base::EmptyString();
}

bool IsStudyExpired(const Study& study, const base::Time& date_time) {
  if (study.has_expiry_date()) {
    const base::Time expiry_date =
        ConvertStudyDateToBaseTime(study.expiry_date());
    return date_time >= expiry_date;
  }

  return false;
}

bool ShouldAddStudy(const Study& study,
                    const ClientFilterableState& client_state) {
  if (study.has_filter()) {
    if (!CheckStudyChannel(study.filter(), client_state.channel)) {
      DVLOG(1) << "Filtered out study " << study.name() << " due to channel.";
      return false;
    }

    if (!CheckStudyFormFactor(study.filter(), client_state.form_factor)) {
      DVLOG(1) << "Filtered out study " << study.name() <<
                  " due to form factor.";
      return false;
    }

    if (!CheckStudyLocale(study.filter(), client_state.locale)) {
      DVLOG(1) << "Filtered out study " << study.name() << " due to locale.";
      return false;
    }

    if (!CheckStudyPlatform(study.filter(), client_state.platform)) {
      DVLOG(1) << "Filtered out study " << study.name() << " due to platform.";
      return false;
    }

    if (!CheckStudyVersion(study.filter(), client_state.version)) {
      DVLOG(1) << "Filtered out study " << study.name() << " due to version.";
      return false;
    }

    if (!CheckStudyStartDate(study.filter(), client_state.reference_date)) {
      DVLOG(1) << "Filtered out study " << study.name() <<
                  " due to start date.";
      return false;
    }

    if (!CheckStudyEndDate(study.filter(), client_state.reference_date)) {
      DVLOG(1) << "Filtered out study " << study.name() << " due to end date.";
      return false;
    }

    if (!CheckStudyHardwareClass(study.filter(), client_state.hardware_class)) {
      DVLOG(1) << "Filtered out study " << study.name() <<
                  " due to hardware_class.";
      return false;
    }

    if (!CheckStudyLowEndDevice(study.filter(),
                                client_state.is_low_end_device)) {
      DVLOG(1) << "Filtered out study " << study.name()
               << " due to is_low_end_device.";
      return false;
    }

    if (!CheckStudyEnterprise(study.filter(), client_state)) {
      DVLOG(1) << "Filtered out study " << study.name()
               << " due to enterprise state.";
      return false;
    }

    if (!CheckStudyOSVersion(study.filter(), client_state.os_version)) {
      DVLOG(1) << "Filtered out study " << study.name()
               << " due to os_version.";
      return false;
    }

    const std::string& country = GetClientCountryForStudy(study, client_state);
    if (!CheckStudyCountry(study.filter(), country)) {
      DVLOG(1) << "Filtered out study " << study.name() << " due to country.";
      return false;
    }
  }

  DVLOG(1) << "Kept study " << study.name() << ".";
  return true;
}

}  // namespace internal

void FilterAndValidateStudies(const VariationsSeed& seed,
                              const ClientFilterableState& client_state,
                              std::vector<ProcessedStudy>* filtered_studies) {
  DCHECK(client_state.version.IsValid());

  // Add expired studies (in a disabled state) only after all the non-expired
  // studies have been added (and do not add an expired study if a corresponding
  // non-expired study got added). This way, if there's both an expired and a
  // non-expired study that applies, the non-expired study takes priority.
  std::set<std::string> created_studies;
  std::vector<const Study*> expired_studies;

  for (int i = 0; i < seed.study_size(); ++i) {
    const Study& study = seed.study(i);
    if (!internal::ShouldAddStudy(study, client_state))
      continue;

    if (internal::IsStudyExpired(study, client_state.reference_date)) {
      expired_studies.push_back(&study);
    } else if (!base::Contains(created_studies, study.name())) {
      ProcessedStudy::ValidateAndAppendStudy(&study, false, filtered_studies);
      created_studies.insert(study.name());
    }
  }

  for (size_t i = 0; i < expired_studies.size(); ++i) {
    if (!base::Contains(created_studies, expired_studies[i]->name())) {
      ProcessedStudy::ValidateAndAppendStudy(expired_studies[i], true,
                                             filtered_studies);
    }
  }
}

}  // namespace variations
