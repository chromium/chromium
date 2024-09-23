// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_METRICS_SYSTEM_SESSION_ANALYZER_SYSTEM_SESSION_ANALYZER_WIN_H_
#define COMPONENTS_METRICS_SYSTEM_SESSION_ANALYZER_SYSTEM_SESSION_ANALYZER_WIN_H_

#include <windows.h>

#include <winevt.h>

#include <map>
#include <memory>
#include <utility>
#include <vector>

#include "base/gtest_prod_util.h"
#include "base/time/time.h"

namespace metrics {

// Analyzes system session events for unclean sessions. Initialization is
// expensive and therefore done lazily, as the analyzer is instantiated before
// knowing whether it will be used.
class SystemSessionAnalyzer {
 public:
  enum Status {
    CLEAN = 0,
    UNCLEAN = 1,
    OUTSIDE_RANGE = 2,
    INITIALIZE_FAILED = 3,
    FETCH_EVENTS_FAILED = 4,
    PROCESS_SESSION_FAILED = 5,
    INSUFFICIENT_DATA = 6,
  };

  // Track internal details of what went wrong.
  enum class ExtendedStatus {
    NO_FAILURE = 0,
    RENDER_EVENT_FAILURE = 1,
    ATTRIBUTE_CNT_MISMATCH = 2,
    EXPECTED_INT16_TYPE = 3,
    EXPECTED_FILETIME_TYPE = 4,
    RETRIEVE_EVENTS_FAILURE = 5,
    GET_EVENT_INFO_FAILURE = 6,
    EVTQUERY_FAILED = 7,
    CREATE_RENDER_CONTEXT_FAILURE = 8,
    FETCH_EVENTS_FAILURE = 9,
    EVENT_COUNT_MISMATCH = 10,
    SESSION_START_MISMATCH = 11,
    COVERAGE_START_ORDER_FAILURE = 12,
    EVENT_ORDER_FAILURE = 13,
    UNEXPECTED_START_EVENT_TYPE = 14,
    UNEXPECTED_END_EVENT_TYPE = 15,
  };

  ExtendedStatus GetExtendedFailureStatus() const;
  // Set an extended failure status code for easier diagnosing of test failures.
  // The first extended status code is retained.
  void SetExtendedFailureStatus(ExtendedStatus);

  // Minimal information about a log event.
  struct EventInfo {
    uint16_t event_id;
    base::Time event_time;
  };

  // Creates a SystemSessionAnalyzer that will analyze system sessions based on
  // events pertaining to as many as |max_session_cnt| of the most recent system
  // sessions.
  explicit SystemSessionAnalyzer(uint32_t max_session_cnt);

  SystemSessionAnalyzer(const SystemSessionAnalyzer&) = delete;
  SystemSessionAnalyzer& operator=(const SystemSessionAnalyzer&) = delete;

  virtual ~SystemSessionAnalyzer();

  // Returns an analysis status for the system session that contains
  // |timestamp|.
  virtual Status IsSessionUnclean(base::Time timestamp);

 protected:
  // Queries for the next |requested_events|. On success, returns true and
  // |event_infos| contains up to |requested_events| events ordered from newest
  // to oldest.
  // Returns false otherwise. Virtual for unit testing.
  virtual bool FetchEvents(size_t requested_events,
                           std::vector<EventInfo>* event_infos);

 private:
  struct EvtHandleCloser {
    using pointer = EVT_HANDLE;
    void operator()(EVT_HANDLE handle) const {
      if (handle)
        ::EvtClose(handle);
    }
  };
  using EvtHandle = std::unique_ptr<EVT_HANDLE, EvtHandleCloser>;

  FRIEND_TEST_ALL_PREFIXES(SystemSessionAnalyzerTest, FetchEvents);

  bool EnsureInitialized();
  bool EnsureHandlesOpened();
  bool Initialize();
  // Validates that |end| and |start| have sane event IDs and event times.
  // Updates |coverage_start_| and adds the session to unclean_sessions_
  // as appropriate.
  bool ProcessSession(const EventInfo& end, const EventInfo& start);

  bool GetEventInfo(EVT_HANDLE context,
                    EVT_HANDLE event,
                    SystemSessionAnalyzer::EventInfo* info);
  EvtHandle CreateRenderContext();

  // The maximal number of sessions to query events for.
  uint32_t max_session_cnt_;
  uint32_t sessions_queried_;

  bool initialized_ = false;
  bool init_success_ = false;

  // A handle to the query, valid after a successful initialize.
  EvtHandle query_handle_;
  // A handle to the event render context, valid after a successful initialize.
  EvtHandle render_context_;

  // Information about unclean sessions: start time to session duration.
  std::map<base::Time, base::TimeDelta> unclean_sessions_;

  // Timestamp of the oldest event.
  base::Time coverage_start_;

  // Track details of what failures occurred.
  ExtendedStatus extended_status_ = ExtendedStatus::NO_FAILURE;
};

}  // namespace metrics

#endif  // COMPONENTS_METRICS_SYSTEM_SESSION_ANALYZER_SYSTEM_SESSION_ANALYZER_WIN_H_
