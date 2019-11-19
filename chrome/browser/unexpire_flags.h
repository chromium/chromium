// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UNEXPIRE_FLAGS_H_
#define CHROME_BROWSER_UNEXPIRE_FLAGS_H_

#include "base/callback.h"
#include "base/feature_list.h"

namespace flags {

extern const base::Feature kUnexpireFlagsM78;
extern const base::Feature kUnexpireFlagsM80;

bool IsFlagExpired(const char* internal_name);

namespace testing {

using FlagPredicate = base::RepeatingCallback<bool(const std::string&)>;

void SetFlagExpiredPredicate(FlagPredicate predicate);

}  // namespace testing

}  // namespace flags

#endif  // CHROME_BROWSER_UNEXPIRE_FLAGS_H_
