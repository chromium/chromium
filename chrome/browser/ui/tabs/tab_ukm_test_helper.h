// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_TAB_UKM_TEST_HELPER_H_
#define CHROME_BROWSER_UI_TABS_TAB_UKM_TEST_HELPER_H_

#include <stdint.h>

#include <map>
#include <optional>
#include <string>
#include <vector>

#include "components/ukm/test_ukm_recorder.h"

class GURL;

// A UKM entry consists of named metrics with int64_t values. Use a map to
// specify expected metrics to test against an actual entry for tests.
// A value of `nullopt` implies a value shouldn't exist for the given metric
// name.
using UkmMetricMap = std::map<const char*, std::optional<int64_t>>;

// Helper class to check entries have been logged as expected into UKM.
// Tests use this by validating new entries after they are logged. The helper
// skips already-validated entries when checking new entries, and expects new
// entries to be validated in the order they were logged. This ensures
// unexpected entries are not logged in between expected entries.
class UkmEntryChecker {
 public:
  UkmEntryChecker();
  UkmEntryChecker(const UkmEntryChecker&) = delete;
  UkmEntryChecker& operator=(const UkmEntryChecker&) = delete;
  ~UkmEntryChecker();

  // Expects that the next untested entry for `entry_name` matches the value
  // and the given URL if `source_url` is not empty.
  // Use this function to verify a single expected event.
  // This function increments `num_entries_[entry_name]` by 1, so entries after
  // this one will still be considered new/untested.
  void ExpectNewEntry(const std::string& entry_name,
                      const GURL& source_url,
                      const UkmMetricMap& expected_metrics);

  // Expects that `expected_entries.size()` new entries have been recorded for
  // `entry_name`, in any order. For each expected entry, checks that its
  // metrics match one of the newly recorded entries.
  // Use this function when expecting multiple entries to be logged at once.
  void ExpectNewEntries(const std::string& entry_name,
                        const std::vector<UkmMetricMap>& expected_entries);

  // Returns number of new entries that have been recorded for `entry_name`.
  // Entries are considered new until they have been validated with
  // ExpectNewEntries() or similar.
  // Thus, this returns the difference between the number of entries in UKM and
  // the number of entries that have been validated.
  int NumNewEntriesRecorded(const std::string& entry_name) const;

 private:
  // Returns number of entries for `entry_name`.
  size_t NumEntries(const std::string& entry_name) const;

  ukm::TestAutoSetUkmRecorder ukm_recorder_;

  // Keyed by entry name, and tracks the expected number of entries to ensure we
  // don't log duplicate or incorrect entries.
  // `num_entries_` records the number of entries that have been expected via
  // calls to ExpectNewEntries() or similar.
  std::map<std::string, size_t> num_entries_;
};

#endif  // CHROME_BROWSER_UI_TABS_TAB_UKM_TEST_HELPER_H_
