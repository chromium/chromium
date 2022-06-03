// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Implementation of the collection of a stability file to a protocol buffer.

#ifndef COMPONENTS_BROWSER_WATCHER_ACTIVITY_REPORT_EXTRACTOR_H_
#define COMPONENTS_BROWSER_WATCHER_ACTIVITY_REPORT_EXTRACTOR_H_

#include "base/debug/activity_analyzer.h"
#include "components/browser_watcher/activity_report.pb.h"

namespace browser_watcher {

// DO NOT REMOVE OR REORDER VALUES. This is logged persistently in a histogram.
enum CollectionStatus {
  NONE = 0,
  SUCCESS = 1,  // Successfully registered a report with Crashpad.
  ANALYZER_CREATION_FAILED = 2,
  DEBUG_FILE_NO_DATA = 3,
  PREPARE_NEW_CRASH_REPORT_FAILED = 4,
  WRITE_TO_MINIDUMP_FAILED = 5,
  DEBUG_FILE_DELETION_FAILED = 6,
  FINISHED_WRITING_CRASH_REPORT_FAILED = 7,
  UNCLEAN_SHUTDOWN = 8,
  UNCLEAN_SESSION = 9,
  COLLECTION_ATTEMPT = 10,
  // New values go here.
  COLLECTION_STATUS_MAX = 11
};

// Extracts a stability report from an existing GlobalActivityAnalyzer.
CollectionStatus Extract(
    std::unique_ptr<base::debug::GlobalActivityAnalyzer> global_analyzer,
    StabilityReport* report);

}  // namespace browser_watcher

#endif  // COMPONENTS_BROWSER_WATCHER_ACTIVITY_REPORT_EXTRACTOR_H_
