
// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/update_client/updater_state.h"

#include <utility>

#include "base/enterprise_util.h"
#include "base/strings/string16.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"

namespace update_client {

// The value of this constant does not reflect its name (i.e. "domainjoined"
// vs something like "isenterprisemanaged") because it is used with omaha.
// After discussion with omaha team it was decided to leave the value as is to
// keep continuity with previous chrome versions.
const char UpdaterState::kIsEnterpriseManaged[] = "domainjoined";

UpdaterState::UpdaterState(bool is_machine) : is_machine_(is_machine) {}

UpdaterState::~UpdaterState() {}

std::unique_ptr<UpdaterState::Attributes> UpdaterState::GetState(
    bool is_machine) {
#if defined(OS_WIN) || (defined(OS_MACOSX) && !defined(OS_IOS))
  UpdaterState updater_state(is_machine);
  updater_state.ReadState();
  return std::make_unique<Attributes>(updater_state.BuildAttributes());
#else
  return nullptr;
#endif  // OS_WIN or Mac
}

#if defined(OS_WIN) || (defined(OS_MACOSX) && !defined(OS_IOS))
void UpdaterState::ReadState() {
  is_enterprise_managed_ = base::IsMachineExternallyManaged();

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  updater_name_ = GetUpdaterName();
  updater_version_ = GetUpdaterVersion(is_machine_);
  last_autoupdate_started_ = GetUpdaterLastStartedAU(is_machine_);
  last_checked_ = GetUpdaterLastChecked(is_machine_);
  is_autoupdate_check_enabled_ = IsAutoupdateCheckEnabled();
  update_policy_ = GetUpdatePolicy();
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)
}
#endif  // OS_WIN or Mac

UpdaterState::Attributes UpdaterState::BuildAttributes() const {
  Attributes attributes;

#if defined(OS_WIN)
  // Only Windows implements this attribute in a meaningful way.
  attributes["ismachine"] = is_machine_ ? "1" : "0";
#endif  // OS_WIN
  attributes[kIsEnterpriseManaged] = is_enterprise_managed_ ? "1" : "0";

  attributes["name"] = updater_name_;

  if (updater_version_.IsValid())
    attributes["version"] = updater_version_.GetString();

  const base::Time now = base::Time::NowFromSystemTime();
  if (!last_autoupdate_started_.is_null())
    attributes["laststarted"] =
        NormalizeTimeDelta(now - last_autoupdate_started_);
  if (!last_checked_.is_null())
    attributes["lastchecked"] = NormalizeTimeDelta(now - last_checked_);

  attributes["autoupdatecheckenabled"] =
      is_autoupdate_check_enabled_ ? "1" : "0";

  DCHECK((update_policy_ >= 0 && update_policy_ <= 3) || update_policy_ == -1);
  attributes["updatepolicy"] = base::NumberToString(update_policy_);

  return attributes;
}

std::string UpdaterState::NormalizeTimeDelta(const base::TimeDelta& delta) {
  const base::TimeDelta two_weeks = base::TimeDelta::FromDays(14);
  const base::TimeDelta two_months = base::TimeDelta::FromDays(60);

  std::string val;  // Contains the value to return in hours.
  if (delta <= two_weeks) {
    val = "0";
  } else if (two_weeks < delta && delta <= two_months) {
    val = "408";  // 2 weeks in hours.
  } else {
    val = "1344";  // 2*28 days in hours.
  }

  DCHECK(!val.empty());
  return val;
}

}  // namespace update_client
