// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/base/logging.h"

#include <string.h>

#include "base/location.h"

namespace syncer {

bool VlogIsOnForLocation(const base::Location& from_here, int verbose_level) {
  return (verbose_level <=
          logging::GetVlogLevelHelper(from_here.file_name(),
                                      ::strlen(from_here.file_name())));
}

}  // namespace syncer
