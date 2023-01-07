// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_HISTORY_CORE_BROWSER_HISTORY_CONSTANTS_H_
#define COMPONENTS_HISTORY_CORE_BROWSER_HISTORY_CONSTANTS_H_

#include "base/files/file_path.h"
#include "base/time/time.h"

namespace history {

// filenames
extern const base::FilePath::CharType kFaviconsFilename[];
extern const base::FilePath::CharType kHistoryFilename[];
extern const base::FilePath::CharType kTopSitesFilename[];

// The maximum number of times a page can change it's title during the relevant
// timestamp (page is either loading is has recently loaded as per
// GetTitleSettingWindow() below).
extern const int kMaxTitleChanges;

// The span of time after load is complete during which a page may set its title
// and have the title change be saved in history.
base::TimeDelta GetTitleSettingWindow();

}  // namespace history

#endif  // COMPONENTS_HISTORY_CORE_BROWSER_HISTORY_CONSTANTS_H_
