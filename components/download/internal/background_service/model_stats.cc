// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/download/internal/background_service/model_stats.h"

#include "base/metrics/histogram_functions.h"

namespace download {
namespace stats {
namespace {

// Converts Entry::State to histogram suffix.
// Should maps to suffix string in histograms.xml.
std::string EntryStateToHistogramSuffix(Entry::State state) {
  std::string suffix;
  switch (state) {
    case Entry::State::NEW:
      return "New";
    case Entry::State::AVAILABLE:
      return "Available";
    case Entry::State::ACTIVE:
      return "Active";
    case Entry::State::PAUSED:
      return "Paused";
    case Entry::State::COMPLETE:
      return "Complete";
    case Entry::State::COUNT:
      break;
  }
  NOTREACHED_IN_MIGRATION();
  return std::string();
}

// Helper method to log the number of entries under a particular state.
void LogDatabaseRecords(download::Entry::State state, uint32_t record_count) {
  std::string name("Download.Service.Db.Records");
  name.append(".").append(EntryStateToHistogramSuffix(state));
  base::UmaHistogramCustomCounts(name, record_count, 1, 500, 50);
}

}  // namespace

void LogModelOperationResult(ModelAction action, bool success) {
  if (success) {
    base::UmaHistogramEnumeration("Download.Service.Db.Operation.Success",
                                  action);
  } else {
    base::UmaHistogramEnumeration("Download.Service.Db.Operation.Failure",
                                  action);
  }
}

void LogEntries(std::map<Entry::State, uint32_t>& entries_count) {
  uint32_t total_records = 0;
  for (const auto& entry_count : entries_count)
    total_records += entry_count.second;

  // Total number of records in database.
  base::UmaHistogramCustomCounts("Download.Service.Db.Records", total_records,
                                 1, 500, 50);

  // Number of records for each Entry::State.
  for (Entry::State state = Entry::State::NEW; state != Entry::State::COUNT;
       state = (Entry::State)((int)(state) + 1)) {
    LogDatabaseRecords(state, entries_count[state]);
  }
}

}  // namespace stats
}  // namespace download
