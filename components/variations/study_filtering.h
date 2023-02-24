// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VARIATIONS_STUDY_FILTERING_H_
#define COMPONENTS_VARIATIONS_STUDY_FILTERING_H_

#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "base/component_export.h"
#include "base/time/time.h"
#include "base/version.h"
#include "components/variations/client_filterable_state.h"
#include "components/variations/processed_study.h"
#include "components/variations/proto/study.pb.h"
#include "components/variations/proto/variations_seed.pb.h"
#include "components/variations/variations_layers.h"

namespace variations {

struct ClientFilterableState;

// Internal functions exposed for testing purposes only.
namespace internal {

// Checks whether a study is applicable for the given |channel| per |filter|.
COMPONENT_EXPORT(VARIATIONS)
bool CheckStudyChannel(const Study::Filter& filter, Study::Channel channel);

// Checks whether a study is applicable for the given |form_factor| per
// |filter|.
COMPONENT_EXPORT(VARIATIONS)
bool CheckStudyFormFactor(const Study::Filter& filter,
                          Study::FormFactor form_factor);

// Checks whether a study is applicable for the given |hardware_class| per
// |filter|.
COMPONENT_EXPORT(VARIATIONS)
bool CheckStudyHardwareClass(const Study::Filter& filter,
                             const std::string& hardware_class);

// Checks whether a study is applicable for the given |locale| per |filter|.
COMPONENT_EXPORT(VARIATIONS)
bool CheckStudyLocale(const Study::Filter& filter, const std::string& locale);

// Checks whether a study is applicable for the given |country| per |filter|.
COMPONENT_EXPORT(VARIATIONS)
bool CheckStudyCountry(const Study::Filter& filter, const std::string& country);

// Checks whether a study is applicable for the given |platform| per |filter|.
COMPONENT_EXPORT(VARIATIONS)
bool CheckStudyPlatform(const Study::Filter& filter, Study::Platform platform);

// Checks whether a study is applicable given |is_low_end_device| per |filter|.
COMPONENT_EXPORT(VARIATIONS)
bool CheckStudyLowEndDevice(const Study::Filter& filter,
                            bool is_low_end_device);

// Checks whether a study is applicable given the ChromeVariations policy value.
COMPONENT_EXPORT(VARIATIONS)
bool CheckStudyPolicyRestriction(const Study::Filter& filter,
                                 RestrictionPolicy policy_restriction);

// Checks whether a study is applicable for the given date/time per |filter|.
COMPONENT_EXPORT(VARIATIONS)
bool CheckStudyStartDate(const Study::Filter& filter,
                         const base::Time& date_time);

// Checks whether a study is applicable for the given date/time per |filter|.
COMPONENT_EXPORT(VARIATIONS)
bool CheckStudyEndDate(const Study::Filter& filter,
                       const base::Time& date_time);

// Checks whether a study is applicable for the given version per |filter|.
COMPONENT_EXPORT(VARIATIONS)
bool CheckStudyVersion(const Study::Filter& filter,
                       const base::Version& version);

// Checks whether a study is applicable for the given OS version per |filter|.
COMPONENT_EXPORT(VARIATIONS)
bool CheckStudyOSVersion(const Study::Filter& filter,
                         const base::Version& os_version);

// Checks whether a study is applicable given |is_enterprise| per |filter|.
COMPONENT_EXPORT(VARIATIONS)
bool CheckStudyEnterprise(const Study::Filter& filter,
                          const ClientFilterableState& client_state);

// Checks whether a study is applicable for the given Google group memberships
// per |filter|.
COMPONENT_EXPORT(VARIATIONS)
bool CheckStudyGoogleGroup(const Study::Filter& filter,
                           const ClientFilterableState& client_state);

// Returns the country that should be used for filtering this study, depending
// on whether the study has session or permanent consistency.
COMPONENT_EXPORT(VARIATIONS)
const std::string& GetClientCountryForStudy(
    const Study& study,
    const ClientFilterableState& client_state);

// Checks whether |study| is expired using the given date/time.
COMPONENT_EXPORT(VARIATIONS)
bool IsStudyExpired(const Study& study, const base::Time& date_time);

// Returns whether |study| should be disabled according to the restriction
// parameters in the |config|.
COMPONENT_EXPORT(VARIATIONS)
bool ShouldAddStudy(const ProcessedStudy& processed_study,
                    const ClientFilterableState& client_state,
                    const VariationsLayers& layers);

}  // namespace internal

// Validates and preprocesses studies in |seed|, filters them according to
// the |client_state| and |layers|, and ensures the result has at most one
// study with the same name.
COMPONENT_EXPORT(VARIATIONS)
std::vector<ProcessedStudy> FilterAndValidateStudies(
    const VariationsSeed& seed,
    const ClientFilterableState& client_state,
    const VariationsLayers& layers);

}  // namespace variations

#endif  // COMPONENTS_VARIATIONS_STUDY_FILTERING_H_
