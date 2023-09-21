// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/safety_hub/safety_hub_constants.h"
#include "base/time/time.h"

namespace safety_hub {

const char kCardHeaderKey[] = "header";
const char kCardSubheaderKey[] = "subheader";
const char kCardStateKey[] = "state";

const base::TimeDelta kMinTimeBetweenPasswordChecks = base::Hours(1);
const base::TimeDelta kPasswordCheckOverdueTimeWindow = base::Hours(1);

}  // namespace safety_hub
