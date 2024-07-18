// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/setup/update_active_setup_version_work_item.h"

#include <stdint.h>

#include <algorithm>
#include <vector>

#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"

namespace {

// The major version and first component of the version identifying the work
// done by setup.exe --configure-user-settings on user login by way of Active
// Setup.  Increase this value if the work done when handling Active Setup
// should be executed again for all existing users.
#define ACTIVE_SETUP_MAJOR_VERSION 43

#define AsWString2(m) L#m
#define AsWString(m) AsWString2(m)

constexpr wchar_t kActiveSetupMajorVersion[] =
    AsWString(ACTIVE_SETUP_MAJOR_VERSION);

#undef AsWString
#undef AsWString2

}  // namespace

UpdateActiveSetupVersionWorkItem::UpdateActiveSetupVersionWorkItem(
    const std::wstring& active_setup_path,
    Operation operation)
    : set_reg_value_work_item_(
          HKEY_LOCAL_MACHINE,
          active_setup_path,
          WorkItem::kWow64Default,
          L"Version",
          base::BindOnce(
              &UpdateActiveSetupVersionWorkItem::GetUpdatedActiveSetupVersion,
              base::Unretained(this))),
      operation_(operation) {}

bool UpdateActiveSetupVersionWorkItem::DoImpl() {
  set_reg_value_work_item_.set_best_effort(best_effort());
  set_reg_value_work_item_.set_rollback_enabled(rollback_enabled());
  return set_reg_value_work_item_.Do();
}

void UpdateActiveSetupVersionWorkItem::RollbackImpl() {
  set_reg_value_work_item_.Rollback();
}

std::wstring UpdateActiveSetupVersionWorkItem::GetUpdatedActiveSetupVersion(
    const std::wstring& existing_version) {
  std::vector<std::wstring> version_components = base::SplitString(
      existing_version, L",", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);

  // If |existing_version| was empty or otherwise corrupted, turn it into a
  // valid one by extending with up to four zeros or truncating to only four
  // components.
  if (version_components.size() != 4U)
    version_components.resize(4U, L"0");

  uint32_t previous_major;
  if (!base::StringToUint(version_components[MAJOR], &previous_major))
    previous_major = 0;

  // Unconditionally update the major version.
  version_components[MAJOR] = kActiveSetupMajorVersion;

  // Clear the other components if the major version increased. No extra work is
  // needed for UPDATE_AND_BUMP_SELECTIVE_TRIGGER in this case since all
  // users will re-run active setup.
  if (ACTIVE_SETUP_MAJOR_VERSION > previous_major) {
    std::fill_n(++version_components.begin(), 3, std::wstring(L"0"));
  } else if (operation_ == UPDATE_AND_BUMP_SELECTIVE_TRIGGER) {
    uint32_t previous_value;
    if (!base::StringToUint(version_components[SELECTIVE_TRIGGER],
                            &previous_value)) {
      LOG(WARNING) << "Couldn't process previous SELECTIVE_TRIGGER Active "
                      "Setup version component: "
                   << version_components[SELECTIVE_TRIGGER];
      previous_value = 0;
    }
    version_components[SELECTIVE_TRIGGER] =
        base::NumberToWString(previous_value + 1);
  }

  return base::JoinString(version_components, L",");
}
