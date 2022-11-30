// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UTILITY_SAFE_BROWSING_MAC_DMG_ANALYZER_H_
#define CHROME_UTILITY_SAFE_BROWSING_MAC_DMG_ANALYZER_H_

#include "base/files/file.h"

namespace safe_browsing {

struct ArchiveAnalyzerResults;

namespace dmg {

class DMGIterator;

// Analyzes the given |dmg_file| for executable content and places the results
// in |results|.
void AnalyzeDMGFile(base::File dmg_file, ArchiveAnalyzerResults* results);

// Helper function exposed for testing. Called by the above overload.
void AnalyzeDMGFile(DMGIterator* iterator, ArchiveAnalyzerResults* results);

}  // namespace dmg
}  // namespace safe_browsing

#endif  // CHROME_UTILITY_SAFE_BROWSING_MAC_DMG_ANALYZER_H_
