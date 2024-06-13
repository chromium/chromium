// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/variations/study_filtering.h"

#include <stddef.h>
#include <stdint.h>

#include <cstdint>
#include <functional>
#include <set>
#include <string_view>

#include "base/containers/contains.h"
#include "base/logging.h"
#include "base/strings/string_util.h"
#include "components/variations/variations_layers.h"
#include "components/variations/variations_seed_processor.h"

namespace variations {
namespace {

// Converts |date_time| in Study date format to base::Time.
base::Time ConvertStudyDateToBaseTime(int64_t date_time) {
  return base::Time::UnixEpoch() + base::Seconds(date_time);
}

// Similar to base::Contains(), but specifically for ASCII strings and
// case-insensitive comparison.
template <typename Collection>
bool ContainsStringIgnoreCaseASCII(const Collection& collection,
                                   const std::string& value) {
  return base::ranges::any_of(collection, [&value](const std::string& s) {
    return base::EqualsCaseInsensitiveASCII(s, value);
  });
}

}  // namespace

namespace internal {

bool CheckStudyChannel(const Study::Filter& filter, Study::Channel channel) {
  // An empty channel list matches all channels.
  if (filter.channel_size() == 0)
    return true;

  return base::Contains(filter.channel(), channel);
}

bool CheckStudyFormFactor(const Study::Filter& filter,
                          Study::FormFactor form_factor) {
  // If both filters are empty, match all values.
  if (filter.form_factor_size() == 0 && filter.exclude_form_factor_size() == 0)
    return true;

  // Allow the |form_factor| if it's in the allowlist.
  // Note if both are specified, the excludelist is ignored. We do not expect
  // both to be present for Chrome due to server-side checks.
  if (filter.form_factor_size() > 0)
    return base::Contains(filter.form_factor(), form_factor);

  // Omit if there is a matching excludelist entry.
  return !base::Contains(filter.exclude_form_factor(), form_factor);
}

bool CheckStudyCpuArchitecture(const Study::Filter& filter,
                               Study::CpuArchitecture cpu_architecture) {
  // If both filters are empty, match all values.
  if (filter.cpu_architecture_size() == 0 &&
      filter.exclude_cpu_architecture_size() == 0) {
    return true;
  }

  // Allow the |cpu_architecture| if it's in the allowlist.
  // Note if both are specified, the excludelist is ignored. We do not expect
  // both to be present for Chrome due to server-side checks.
  if (filter.cpu_architecture_size() > 0)
    return base::Contains(filter.cpu_architecture(), cpu_architecture);

  // Omit if there is a matching excludelist entry.
  return !base::Contains(filter.exclude_cpu_architecture(), cpu_architecture);
}

bool CheckStudyHardwareClass(const Study::Filter& filter,
                             const std::string& hardware_class) {
  // If both filters are empty, match all values.
  if (filter.hardware_class_size() == 0 &&
      filter.exclude_hardware_class_size() == 0) {
    return true;
  }

  // Note: This logic changed in M66. Prior to M66, this used substring
  // comparison logic to match hardware classes. In M66, it was made consistent
  // with other filters.

  // Allow the |hardware_class| if it's in the allowlist.
  // Note if both are specified, the excludelist is ignored. We do not expect
  // both to be present for Chrome due to server-side checks.
  if (filter.hardware_class_size() > 0) {
    return ContainsStringIgnoreCaseASCII(filter.hardware_class(),
                                         hardware_class);
  }

  // Omit if there is a matching excludelist entry.
  return !ContainsStringIgnoreCaseASCII(filter.exclude_hardware_class(),
                                        hardware_class);
}

bool CheckStudyLocale(const Study::Filter& filter, const std::string& locale) {
  // If both filters are empty, match all values.
  if (filter.locale_size() == 0 && filter.exclude_locale_size() == 0)
    return true;

  // Allow the |locale| if it's in the allowlist.
  // Note if both are specified, the excludelist is ignored. We do not expect
  // both to be present for Chrome due to server-side checks.
  if (filter.locale_size() > 0)
    return base::Contains(filter.locale(), locale);

  // Omit if there is a matching excludelist entry.
  return !base::Contains(filter.exclude_locale(), locale);
}

bool CheckStudyCountry(const Study::Filter& filter,
                       const std::string& country) {
  // If both filters are empty, match all values.
  if (filter.country_size() == 0 && filter.exclude_country_size() == 0)
    return true;

  // Allow the |country| if it's in the allowlist.
  // Note if both are specified, the excludelist is ignored. We do not expect
  // both to be present for Chrome due to server-side checks.
  if (filter.country_size() > 0)
    return base::Contains(filter.country(), country);

  // Omit if there is a matching excludelist entry.
  return !base::Contains(filter.exclude_country(), country);
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

bool CheckStudyPolicyRestriction(const Study::Filter& filter,
                                 RestrictionPolicy policy_restriction) {
  switch (policy_restriction) {
    // If the policy is set to no restrictions let any study that is not
    // specifically designated for clients requesting critical studies only.
    case RestrictionPolicy::NO_RESTRICTIONS:
      return filter.policy_restriction() != Study::CRITICAL_ONLY;
    // If the policy is set to only allow critical studies than make sure they
    // have that restriction applied on their Filter.
    case RestrictionPolicy::CRITICAL_ONLY:
      return filter.policy_restriction() != Study::NONE;
    // If the policy is set to not allow any variations then return false
    // regardless of the actual Filter.
    case RestrictionPolicy::ALL:
      return false;
  }
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

bool CheckStudyEnterprise(const Study::Filter& filter,
                          const ClientFilterableState& client_state) {
  return !filter.has_is_enterprise() ||
         filter.is_enterprise() == client_state.IsEnterprise();
}

bool CheckStudyGoogleGroup(const Study::Filter& filter,
                           const ClientFilterableState& client_state) {
  if (filter.google_group_size() == 0 &&
      filter.exclude_google_group_size() == 0) {
    // This study doesn't have any google group configuration, so break early.
    return true;
  }

  // Fetch the groups this client is a member of.
  base::flat_set<uint64_t> client_groups = client_state.GoogleGroups();

  if (filter.google_group_size() > 0 &&
      filter.exclude_google_group_size() > 0) {
    // This is an invalid configuration; reject the study.
    return false;
  }

  if (filter.google_group_size() > 0) {
    for (int64_t filter_group : filter.google_group()) {
      if (base::Contains(client_groups, filter_group)) {
        return true;
      }
    }
    return false;
  }

  if (filter.exclude_google_group_size() > 0) {
    for (int64_t filter_exclude_group : filter.exclude_google_group()) {
      if (base::Contains(client_groups, filter_exclude_group)) {
        return false;
      }
    }
    return true;
  }

  return true;
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

bool ShouldAddStudy(const ProcessedStudy& processed_study,
                    const ClientFilterableState& client_state,
                    const VariationsLayers& layers) {
  const Study& study = *processed_study.study();
  if (study.has_expiry_date()) {
    DVLOG(1) << "Filtered out study " << study.name()
             << " due to unsupported expiry_date field.";
    return false;
  }

  if (study.has_layer()) {
    if (!layers.IsLayerMemberActive(study.layer())) {
      DVLOG(1) << "Filtered out study " << study.name()
               << " due to layer member not being active.";
      return false;
    }

    if (!VariationsLayers::AllowsHighEntropy(study) &&
        layers.ActiveLayerMemberDependsOnHighEntropy(
            study.layer().layer_id())) {
      DVLOG(1)
          << "Filtered out study " << study.name()
          << " due to not allowing a high entropy source yet being a member "
             "of a layer using the default (high) entropy source.";
      return false;
    }
  }

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

    if (!CheckStudyCpuArchitecture(study.filter(),
                                   client_state.cpu_architecture)) {
      DVLOG(1) << "Filtered out study " << study.name()
               << " due to cpu architecture.";
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

    if (!CheckStudyPolicyRestriction(study.filter(),
                                     client_state.policy_restriction)) {
      DVLOG(1) << "Filtered out study " << study.name()
               << " due to policy restriction.";
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

    // Check for enterprise status last as checking whether the client is
    // enterprise can be slow.
    if (!CheckStudyEnterprise(study.filter(), client_state)) {
      DVLOG(1) << "Filtered out study " << study.name()
               << " due to enterprise state.";
      return false;
    }

    if (!CheckStudyGoogleGroup(study.filter(), client_state)) {
      DVLOG(1) << "Filtered out study " << study.name()
               << " due to Google groups membership checks.";
      return false;
    }
  }

  DVLOG(1) << "Kept study " << study.name() << ".";
  return true;
}

}  // namespace internal

std::vector<ProcessedStudy> FilterAndValidateStudies(
    const VariationsSeed& seed,
    const ClientFilterableState& client_state,
    const VariationsLayers& layers) {
  DCHECK(client_state.version.IsValid());

  std::vector<ProcessedStudy> filtered_studies;

  // Don't create two studies with the same name.
  // These `string_view`s contain pointers which point to memory owned by
  // `seed`.
  std::set<std::string_view, std::less<>> created_studies;

  for (const Study& study : seed.study()) {
    ProcessedStudy processed_study;
    if (!processed_study.Init(&study))
      continue;

    if (!internal::ShouldAddStudy(processed_study, client_state, layers))
      continue;

    auto [it, inserted] =
        created_studies.insert(processed_study.study()->name());
    if (!inserted) {
      // The study's name is already in `created_studies`, which means that a
      // study with the same name was already added to `filtered_studies`.
      continue;
    }

    filtered_studies.push_back(processed_study);
  }
  return filtered_studies;
}

}  // namespace variations
