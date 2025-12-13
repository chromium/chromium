// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/regional_capabilities/regional_capabilities_internals_data_holder.h"

#include "base/check_is_test.h"
#include "base/notreached.h"
#include "components/regional_capabilities/access/country_access_reason.h"
#include "components/regional_capabilities/regional_capabilities_service.h"
#include "components/webui/regional_capabilities_internals/constants.h"

namespace regional_capabilities {

namespace {
std::string ProgramToString(Program program) {
  switch (program) {
    case Program::kDefault:
      return "Default";
    case Program::kTaiyaki:
      return "Taiyaki";
    case Program::kWaffle:
      return "Waffle";
  }
  NOTREACHED();
}
}  // namespace

InternalsDataHolder::InternalsDataHolder(
    RegionalCapabilitiesService& regional_capabilities) {
  data_.insert_or_assign(
      kActiveProgramNameKey,
      ProgramToString(
          regional_capabilities.GetActiveProgramSettings().program));

  data_.insert_or_assign(
      kActiveCountryCodeKey,
      regional_capabilities.GetCountryIdInternal().CountryCode());

  data_.insert_or_assign(
      kPrefsCountryCodeKey,
      regional_capabilities.GetPersistedCountryId().CountryCode());

#if BUILDFLAG(IS_ANDROID)
  data_.insert_or_assign(
      kDeviceDeterminedProgramKey,
      ProgramToString(regional_capabilities.client_->GetDeviceProgram()));
#endif
}

InternalsDataHolder::~InternalsDataHolder() = default;

InternalsDataHolder::InternalsDataHolder(const InternalsDataHolder& other) =
    default;

InternalsDataHolder& InternalsDataHolder::operator=(
    const InternalsDataHolder& other) = default;

bool InternalsDataHolder::operator==(const InternalsDataHolder& other) const {
  return data_ == other.data_;
}

const base::flat_map<std::string, std::string>&
InternalsDataHolder::GetRestricted(CountryAccessKey access_key) const {
  // TODO(crbug.com/328040066): Record access to UMA.
  return data_;
}

const base::flat_map<std::string, std::string>&
InternalsDataHolder::GetForTesting() const {
  CHECK_IS_TEST();
  return data_;
}

}  // namespace regional_capabilities
