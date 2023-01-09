// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_INSTALLER_SETUP_USER_HIVE_VISITOR_H_
#define CHROME_INSTALLER_SETUP_USER_HIVE_VISITOR_H_

#include "base/functional/callback_forward.h"

namespace base {
namespace win {
class RegKey;
}
}  // namespace base

namespace installer {

// The visitor callback invoked for each user's registry hive by
// |VisitUserHives|. |user_sid| is the user SID being visited. |key| is the root
// of that user's registry hive. Implementations return |true| to indicate that
// the visits should continue, or |false| to indicate that visits should stop.
using HiveVisitor = base::RepeatingCallback<bool(const wchar_t* user_sid,
                                                 base::win::RegKey* key)>;

// Runs |visitor| for each local user profile's registry hive.
void VisitUserHives(const HiveVisitor& visitor);

}  // namespace installer

#endif  // CHROME_INSTALLER_SETUP_USER_HIVE_VISITOR_H_
