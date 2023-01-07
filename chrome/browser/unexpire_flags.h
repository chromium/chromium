// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UNEXPIRE_FLAGS_H_
#define CHROME_BROWSER_UNEXPIRE_FLAGS_H_

#include "base/feature_list.h"
#include "base/functional/callback.h"

namespace flags_ui {
class FlagsStorage;
}  // namespace flags_ui

namespace flags {

bool IsFlagExpired(const flags_ui::FlagsStorage* storage,
                   const char* internal_name);

namespace testing {

// Overrides the expiration milestone for a named flag. Useful for tests that
// need to expire a flag that doesn't normally appear in the generated
// expiration table.
void SetFlagExpiration(const std::string& name, int mstone);

}  // namespace testing

}  // namespace flags

#endif  // CHROME_BROWSER_UNEXPIRE_FLAGS_H_
