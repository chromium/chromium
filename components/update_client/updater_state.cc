
// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/update_client/updater_state.h"

#include <string>
#include <utility>

#include "base/enterprise_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"

namespace update_client {

UpdaterState::UpdaterState(bool is_machine) : is_machine_(is_machine) {}

UpdaterState::~UpdaterState() = default;

std::unique_ptr<UpdaterState::Attributes> UpdaterState::GetState(
    bool is_machine) {
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
  UpdaterState updater_state(is_machine);
  updater_state.ReadState();
  return std::make_unique<Attributes>(updater_state.BuildAttributes());
#else
  return nullptr;
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
}

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
void UpdaterState::ReadState() {
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  updater_name_ = GetUpdaterName();
  updater_version_ = GetUpdaterVersion(is_machine_);
  last_autoupdate_started_ = GetUpdaterLastStartedAU(is_machine_);
  last_checked_ = GetUpdaterLastChecked(is_machine_);
  is_autoupdate_check_enabled_ = IsAutoupdateCheckEnabled();
  update_policy_ = GetUpdatePolicy();
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)
}
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)

UpdaterState::Attributes UpdaterState::BuildAttributes() const {
  Attributes attributes;

#if BUILDFLAG(IS_WIN)
  // Only Windows implements this attribute in a meaningful way.
  attributes["ismachine"] = is_machine_ ? "1" : "0";
#endif  // BUILDFLAG(IS_WIN)

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
  const base::TimeDelta two_weeks = base::Days(14);
  const base::TimeDelta two_months = base::Days(56);

  std::string val;  // Contains the value to return in hours.
  if (delta <= two_weeks) {
    val = "0";
  } else if (two_weeks < delta && delta <= two_months) {
    val = "336";  // 2 weeks in hours.
  } else {
    val = "1344";  // 2*28 days in hours.
  }

  DCHECK(!val.empty());
  return val;
}

}  // namespace update_client
