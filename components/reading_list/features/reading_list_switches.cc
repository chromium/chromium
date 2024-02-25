// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/reading_list/features/reading_list_switches.h"

#include "base/command_line.h"
#include "base/feature_list.h"
#include "build/build_config.h"
#include "components/reading_list/features/reading_list_buildflags.h"
#include "components/sync/base/features.h"

namespace reading_list {
namespace switches {

BASE_FEATURE(kReadLaterBackendMigration,
             "ReadLaterBackendMigration",
             base::FEATURE_DISABLED_BY_DEFAULT);

}  // namespace switches
}  // namespace reading_list
