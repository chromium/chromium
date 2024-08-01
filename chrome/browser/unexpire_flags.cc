// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/browser/unexpire_flags.h"

#include "base/containers/contains.h"
#include "base/containers/flat_map.h"
#include "base/no_destructor.h"
#include "chrome/browser/expired_flags_list.h"
#include "chrome/browser/unexpire_flags_gen.h"
#include "chrome/common/chrome_version.h"
#include "components/flags_ui/flags_storage.h"

namespace flags {

namespace {

using FlagNameToExpirationMap = base::flat_map<std::string, int>;

static FlagNameToExpirationMap* GetFlagExpirationOverrideMap() {
  static base::NoDestructor<FlagNameToExpirationMap> map;
  return map.get();
}

int ExpirationMilestoneForFlag(const char* flag) {
  if (base::Contains(*GetFlagExpirationOverrideMap(), flag)) {
    return GetFlagExpirationOverrideMap()->at(flag);
  }

  for (int i = 0; kExpiredFlags[i].name; ++i) {
    const ExpiredFlag* f = &kExpiredFlags[i];
    if (strcmp(f->name, flag)) {
      continue;
    }

    // To keep the size of the expired flags list down,
    // //tools/flags/generate_expired_flags.py doesn't emit flags with expiry
    // mstone -1; it makes no sense for these flags to be in the expiry list
    // anyway. However, if a bug did cause that to happen, and this function
    // didn't handle that case, all flags with expiration -1 would immediately
    // expire, which would be very bad. As such there's an extra error-check
    // here: a DCHECK to catch bugs in the script, and a regular if to ensure we
    // never expire flags that should never expire.
    DCHECK_NE(f->mstone, -1);
    return f->mstone;
  }
  return -1;
}

// This function is a nasty hack - normally, the logic to turn flags into
// feature names happens inside flags_ui::FlagsState, but this function is used
// from the setup code of FlagsState, so it can't rely on FlagsState having been
// set up. As such, we look into the backing FlagsStorage and hardcode how
// enabled flags look inside that storage.
std::set<int> UnexpiredMilestonesFromStorage(
    const flags_ui::FlagsStorage* storage) {
  std::set<int> unexpired;
  for (const auto& f : storage->GetFlags()) {
    int mstone;
    if (sscanf(f.c_str(), "temporary-unexpire-flags-m%d@1", &mstone) == 1) {
      unexpired.insert(mstone);
    }
  }
  return unexpired;
}

}  // namespace

bool IsFlagExpired(const flags_ui::FlagsStorage* storage,
                   const char* internal_name) {
  DCHECK(storage);

  int mstone = ExpirationMilestoneForFlag(internal_name);
  if (mstone == -1) {
    return false;
  }

  // This is extremely horrible:
  //
  // In order to know if a flag is expired or not, normally this function
  // queries the state of base::FeatureList to check whether the unexpire
  // feature for that milestone is enabled. However, when *creating* the initial
  // base::FeatureList instance, these features won't be initialized yet, which
  // leads to this issue:
  //
  // * Assume a flag "foo-bar" for feature FooBar that expires in M83.
  // * Also, assume that temporary-unexpire-flags-m83 is enabled.
  //
  // If both of those are true, then if IsFlagExpired("foo-bar") is called
  // *during* initial feature list setup, it will return true rather than false,
  // which will cause FooBar to be set to its default rather than the
  // non-default value that the flag may be to. This happens because the
  // TemporaryUnexpireFlagsM83 feature hasn't been initialized yet, so it gets
  // treated as its default state (disabled).
  //
  // To deal with that and make this function behave more correctly during
  // FeatureList initialization, also consult the backing FlagsStorage from the
  // FlagsState and look at the temporary-unexpire-flags-m$M flags directly, as
  // well as looking at their features.
  //
  // This still has a problem: during browser startup, if the unexpire feature
  // will be configured by some other mechanism (group policy, etc), that
  // feature's value won't apply in time here and the bug described will happen.
  // In fact, that is a design behavior of the feature system, since flag
  // unexpiry happens during FeatureList initialization.
  // TODO(ellyjones): what might we do about that?
  std::set<int> unexpired_milestones = UnexpiredMilestonesFromStorage(storage);
  if (base::Contains(unexpired_milestones, mstone)) {
    return false;
  }

  // Otherwise, the flag is expired if its expiration mstone is less than the
  // mstone of this copy of Chromium.
  return mstone < CHROME_VERSION_MAJOR;
}

namespace testing {

void SetFlagExpiration(const std::string& name, int mstone) {
  GetFlagExpirationOverrideMap()->insert_or_assign(name, mstone);
}

}  // namespace testing

}  // namespace flags
