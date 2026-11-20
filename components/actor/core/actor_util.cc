// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/actor/core/actor_util.h"

#include "base/check.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/strings/string_util.h"
#include "components/actor/core/actor_features.h"
#include "components/actor/core/actor_switches.h"

namespace actor {

bool IsActorSafetyCheckDisabled() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kDisableActorSafetyChecks);
}

bool IsNavigationGatingEnabled() {
  return !IsActorSafetyCheckDisabled() &&
         base::FeatureList::IsEnabled(kGlicCrossOriginNavigationGating);
}

void TruncateValidUtf8ToByteSize(std::string& str, size_t max_bytes) {
  DCHECK(base::IsStringUTF8AllowingNoncharacters(str));
  if (str.size() <= max_bytes) {
    return;
  }
  size_t end = max_bytes;
  while (end > 0 && (static_cast<unsigned char>(str[end]) & 0xC0) == 0x80) {
    --end;
  }
  str.resize(end);
}

}  // namespace actor
