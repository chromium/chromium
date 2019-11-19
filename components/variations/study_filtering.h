// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VARIATIONS_STUDY_FILTERING_H_
#define COMPONENTS_VARIATIONS_STUDY_FILTERING_H_

#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "base/time/time.h"
#include "base/version.h"
#include "components/variations/processed_study.h"
#include "components/variations/proto/study.pb.h"
#include "components/variations/proto/variations_seed.pb.h"

namespace variations {

struct ClientFilterableState;

// Internal functions exposed for testing purposes only.
namespace internal {

// Checks whether a study is applicable for the given |channel| per |filter|.
bool CheckStudyChannel(const Study::Filter& filter, Study::Channel channel);

// Checks whether a study is applicable for the given |form_factor| per
// |filter|.
bool CheckStudyFormFactor(const Study::Filter& filter,
                          Study::FormFactor form_factor);

// Checks whether a study is applicable for the given |hardware_class| per
// |filter|.
bool CheckStudyHardwareClass(const Study::Filter& filter,
                             const std::string& hardware_class);

// Checks whether a study is applicable for the given |locale| per |filter|.
bool CheckStudyLocale(const Study::Filter& filter, const std::string& locale);

// Checks whether a study is applicable for the given |platform| per |filter|.
bool CheckStudyPlatform(const Study::Filter& filter, Study::Platform platform);

// Checks whether a study is applicable given |is_low_end_device| per |filter|.
bool CheckStudyLowEndDevice(const Study::Filter& filter,
                            bool is_low_end_device);

// Checks whether a study is applicable given |is_enterprise| per |filter|.
bool CheckStudyEnterprise(const Study::Filter& filter,
                          const ClientFilterableState& client_state);

// Checks whether a study is applicable for the given date/time per |filter|.
bool CheckStudyStartDate(const Study::Filter& filter,
                         const base::Time& date_time);

// Checks whether a study is applicable for the given date/time per |filter|.
bool CheckStudyEndDate(const Study::Filter& filter,
                       const base::Time& date_time);

// Checks whether a study is applicable for the given version per |filter|.
bool CheckStudyVersion(const Study::Filter& filter,
                       const base::Version& version);

// Checks whether a study is applicable for the given OS version per |filter|.
bool CheckStudyOSVersion(const Study::Filter& filter,
                         const base::Version& os_version);

// Checks whether a study is applicable for the given |country| per |filter|.
bool CheckStudyCountry(const Study::Filter& filter, const std::string& country);

// Returns the country that should be used for filtering this study, depending
// on whether the study has session or permanent consistency.
const std::string& GetClientCountryForStudy(
    const Study& study,
    const ClientFilterableState& client_state);

// Checks whether |study| is expired using the given date/time.
bool IsStudyExpired(const Study& study, const base::Time& date_time);

// Returns whether |study| should be disabled according to the restriction
// parameters in the |config|.
bool ShouldAddStudy(const Study& study,
                    const ClientFilterableState& client_state);

}  // namespace internal

// Filters the list of studies in |seed| according ot the |client_state|, and
// validates and pre-processes them, adding any kept studies to the
// |filtered_studies| list. Ensures that the resulting list will not have more
// than one study with the same name.
void FilterAndValidateStudies(const VariationsSeed& seed,
                              const ClientFilterableState& client_state,
                              std::vector<ProcessedStudy>* filtered_studies);

}  // namespace variations

#endif  // COMPONENTS_VARIATIONS_STUDY_FILTERING_H_
