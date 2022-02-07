// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_SEARCH_FILES_JUSTIFICATIONS_H_
#define CHROME_BROWSER_UI_APP_LIST_SEARCH_FILES_JUSTIFICATIONS_H_

#include <string>

#include "base/files/file_path.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace app_list {

// Returns an appropriate justification string for displaying |path| as a file
// suggestion, for example "you opened yesterday".
//
// This uses the most recent of the edited and opened times, and returns a
// different message for times within the last few minutes, day, two days, week,
// and month. If there is an error stating |path| or the time is longer than a
// month ago, nullopt is returned.
absl::optional<std::u16string> GetJustificationString(
    const base::FilePath& path);

}  // namespace app_list

#endif  // CHROME_BROWSER_UI_APP_LIST_SEARCH_FILES_JUSTIFICATIONS_H_
