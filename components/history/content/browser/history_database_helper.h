// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_HISTORY_CONTENT_BROWSER_HISTORY_DATABASE_HELPER_H_
#define COMPONENTS_HISTORY_CONTENT_BROWSER_HISTORY_DATABASE_HELPER_H_

#include "components/version_info/channel.h"

namespace base {
class FilePath;
}

namespace history {

struct HistoryDatabaseParams;

// Returns a HistoryDatabaseParams for `history_dir`.
HistoryDatabaseParams HistoryDatabaseParamsForPath(
    const base::FilePath& history_dir,
    version_info::Channel channel);

}  // namespace history

#endif  // COMPONENTS_HISTORY_CONTENT_BROWSER_HISTORY_DATABASE_HELPER_H_
