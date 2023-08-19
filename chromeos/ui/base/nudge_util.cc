// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ui/base/nudge_util.h"

#include "base/strings/stringprintf.h"

namespace chromeos {

namespace {

const char* GetNudgeTimeToActionRange(base::TimeDelta time) {
  if (time <= base::Minutes(1)) {
    return "Within1m";
  }

  return time <= base::Hours(1) ? "Within1h" : "WithinSession";
}

}  // namespace

std::string GetNudgeTimeToActionHistogramName(base::TimeDelta time) {
  return base::StringPrintf("Ash.NotifierFramework.Nudge.TimeToAction.%s",
                            GetNudgeTimeToActionRange(time));
}

}  // namespace chromeos
