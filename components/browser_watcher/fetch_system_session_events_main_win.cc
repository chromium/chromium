// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// A utility for testing locally the retrieval of system session events.

#include <iostream>

#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "components/metrics/system_session_analyzer/system_session_analyzer_win.h"

namespace {

using metrics::SystemSessionAnalyzer;

class SystemSessionEventFetcher : public SystemSessionAnalyzer {
 public:
  explicit SystemSessionEventFetcher() : SystemSessionAnalyzer(0) {}
  using SystemSessionAnalyzer::FetchEvents;
};

}  // namespace

int main(int argc, char** argv) {
  SystemSessionEventFetcher fetcher;
  std::vector<SystemSessionEventFetcher::EventInfo> events;
  // Retrieve events for the last 5 sessions. We expect our own sessions start
  // event, and then 2 events per each preceding session for 11 total.
  if (!fetcher.FetchEvents(11U, &events)) {
    std::cerr << "Failed to fetch events." << std::endl;
    return 1;
  }

  // Print the event ids and times.
  for (const SystemSessionEventFetcher::EventInfo& event : events) {
    base::Time::Exploded exploded = {};
    event.event_time.LocalExplode(&exploded);
    std::string time = base::StringPrintf(
        "%d/%d/%d %d:%02d:%02d", exploded.month, exploded.day_of_month,
        exploded.year, exploded.hour, exploded.minute, exploded.second);
    std::cout << "Event: " << event.event_id << " (" << time << ")"
              << std::endl;
  }

  return 0;
}
