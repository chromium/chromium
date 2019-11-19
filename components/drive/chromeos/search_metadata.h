// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DRIVE_CHROMEOS_SEARCH_METADATA_H_
#define COMPONENTS_DRIVE_CHROMEOS_SEARCH_METADATA_H_

#include <string>
#include <vector>

namespace base {
namespace i18n {
class FixedPatternStringSearchIgnoringCaseAndAccents;
}  // namespace i18n
}  // namespace base

namespace drive {
namespace internal {

// Finds |queries| in |text| while ignoring cases or accents. Cases of non-ASCII
// characters are also ignored; they are compared in the 'Primary Level' of
// http://userguide.icu-project.org/collation/concepts.
// Returns true if |queries| are found. |highlighted_text| will have the
// original
// text with matched portions highlighted with <b> tag (only the first match
// is highlighted). Meta characters are escaped like &lt;. The original
// contents of |highlighted_text| will be lost.
bool FindAndHighlight(
    const std::string& text,
    const std::vector<std::unique_ptr<
        base::i18n::FixedPatternStringSearchIgnoringCaseAndAccents>>& queries,
    std::string* highlighted_text);

}  // namespace internal
}  // namespace drive

#endif  // COMPONENTS_DRIVE_CHROMEOS_SEARCH_METADATA_H_
