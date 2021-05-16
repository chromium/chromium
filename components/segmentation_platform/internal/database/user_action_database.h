// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_DATABASE_USER_ACTION_DATABASE_H_
#define COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_DATABASE_USER_ACTION_DATABASE_H_

#include "base/time/time.h"

namespace segmentation_platform {

// Responsible for storing user action events in a database.
class UserActionDatabase {
 public:
  // Called to write user actions to the database. This is a non-blocking write.
  virtual void WriteUserAction(uint64_t user_action_hash,
                               base::TimeTicks timestamp) = 0;

  virtual ~UserActionDatabase() = default;
};

}  // namespace segmentation_platform

#endif  // COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_DATABASE_USER_ACTION_DATABASE_H_
