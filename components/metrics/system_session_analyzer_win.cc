// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/system_session_analyzer_win.h"

#include "base/stl_util.h"
#include "base/time/time.h"

namespace metrics {

namespace {

// The name of the log channel to query.
const wchar_t kChannelName[] = L"System";

// Event ids of system startup / shutdown events. These were obtained from
// inspection of the System log in Event Viewer on Windows 10:
//   - id 6005: "The Event log service was started."
//   - id 6006: "The Event log service was stopped."
//   - id 6008: "The previous system shutdown at <time> on <date> was
//               unexpected."
const uint16_t kIdSessionStart = 6005U;
const uint16_t kIdSessionEnd = 6006U;
const uint16_t kIdSessionEndUnclean = 6008U;

// An XPATH expression to query for system startup / shutdown events. The query
// is expected to retrieve exactly one event for each startup (kIdSessionStart)
// and one event for each shutdown (either kIdSessionEnd or
// kIdSessionEndUnclean).
const wchar_t kSessionEventsQuery[] =
    L"*[System[Provider[@Name='eventlog']"
    L" and (EventID=6005 or EventID=6006 or EventID=6008)]]";

// XPath expressions to attributes of interest.
const wchar_t kEventIdPath[] = L"Event/System/EventID";
const wchar_t kEventTimePath[] = L"Event/System/TimeCreated/@SystemTime";

// The timeout to use for calls to ::EvtNext.
const uint32_t kTimeoutMs = 5000;

base::Time ULLFileTimeToTime(ULONGLONG time_ulonglong) {
  // Copy low / high parts as FILETIME is not always 64bit aligned.
  ULARGE_INTEGER time;
  time.QuadPart = time_ulonglong;
  FILETIME ft;
  ft.dwLowDateTime = time.LowPart;
  ft.dwHighDateTime = time.HighPart;

  return base::Time::FromFileTime(ft);
}

bool GetEventInfo(EVT_HANDLE context,
                  EVT_HANDLE event,
                  SystemSessionAnalyzer::EventInfo* info) {
  DCHECK(context);
  DCHECK(event);
  DCHECK(info);

  // Retrieve attributes of interest from the event. We expect the context to
  // specify the retrieval of two attributes (event id and event time), each
  // with a specific type.
  const DWORD kAttributeCnt = 2U;
  std::vector<EVT_VARIANT> buffer(kAttributeCnt);
  DWORD buffer_size = kAttributeCnt * sizeof(EVT_VARIANT);
  DWORD buffer_used = 0U;
  DWORD retrieved_attribute_cnt = 0U;
  if (!::EvtRender(context, event, EvtRenderEventValues, buffer_size,
                   buffer.data(), &buffer_used, &retrieved_attribute_cnt)) {
    DLOG(ERROR) << "Failed to render the event.";
    return false;
  }

  // Validate the count and types of the retrieved attributes.
  if ((retrieved_attribute_cnt != kAttributeCnt) ||
      (buffer[0].Type != EvtVarTypeUInt16) ||
      (buffer[1].Type != EvtVarTypeFileTime)) {
    return false;
  }

  info->event_id = buffer[0].UInt16Val;
  info->event_time = ULLFileTimeToTime(buffer[1].FileTimeVal);

  return true;
}

}  // namespace

SystemSessionAnalyzer::SystemSessionAnalyzer(uint32_t max_session_cnt)
    : max_session_cnt_(max_session_cnt), sessions_queried_(0) {}

SystemSessionAnalyzer::~SystemSessionAnalyzer() {}

SystemSessionAnalyzer::Status SystemSessionAnalyzer::IsSessionUnclean(
    base::Time timestamp) {
  if (!EnsureInitialized())
    return FAILED;

  while (timestamp < coverage_start_ && sessions_queried_ < max_session_cnt_) {
    // Fetch the next session start and end events.
    std::vector<EventInfo> events;
    if (!FetchEvents(2U, &events) || events.size() != 2)
      return FAILED;

    if (!ProcessSession(events[0], events[1]))
      return FAILED;

    ++sessions_queried_;
  }

  if (timestamp < coverage_start_)
    return OUTSIDE_RANGE;

  // Get the first session starting after the timestamp.
  std::map<base::Time, base::TimeDelta>::const_iterator it =
      unclean_sessions_.upper_bound(timestamp);
  if (it == unclean_sessions_.begin())
    return CLEAN;  // No prior unclean session.

  // Get the previous session and see if it encompasses the timestamp.
  --it;
  bool is_spanned = (timestamp - it->first) <= it->second;
  return is_spanned ? UNCLEAN : CLEAN;
}

bool SystemSessionAnalyzer::FetchEvents(size_t requested_events,
                                        std::vector<EventInfo>* event_infos) {
  DCHECK(event_infos);

  if (!EnsureHandlesOpened())
    return false;

  DCHECK(query_handle_.get());

  // Retrieve events: 2 events per session, plus the current session's start.
  DWORD desired_event_cnt = requested_events;
  std::vector<EVT_HANDLE> events_raw(desired_event_cnt, nullptr);
  DWORD event_cnt = 0U;
  BOOL success = ::EvtNext(query_handle_.get(), desired_event_cnt,
                           events_raw.data(), kTimeoutMs, 0, &event_cnt);

  // Ensure handles get closed. The MSDN sample seems to imply handles may need
  // to be closed event in if EvtNext failed.
  std::vector<EvtHandle> events(desired_event_cnt);
  for (size_t i = 0; i < event_cnt; ++i)
    events[i].reset(events_raw[i]);

  if (!success) {
    DLOG(ERROR) << "Failed to retrieve events.";
    return false;
  }

  std::vector<EventInfo> event_infos_tmp;
  event_infos_tmp.reserve(event_cnt);

  EventInfo info = {};
  for (size_t i = 0; i < event_cnt; ++i) {
    if (!GetEventInfo(render_context_.get(), events[i].get(), &info))
      return false;
    event_infos_tmp.push_back(info);
  }

  event_infos->swap(event_infos_tmp);
  return true;
}

bool SystemSessionAnalyzer::EnsureInitialized() {
  if (!initialized_) {
    DCHECK(!init_success_);
    init_success_ = Initialize();
    initialized_ = true;
  }

  return init_success_;
}

bool SystemSessionAnalyzer::EnsureHandlesOpened() {
  // Create the event query.
  // Note: requesting events from newest to oldest.
  if (!query_handle_.get()) {
    query_handle_.reset(
        ::EvtQuery(nullptr, kChannelName, kSessionEventsQuery,
                   EvtQueryChannelPath | EvtQueryReverseDirection));
    if (!query_handle_.get()) {
      DLOG(ERROR) << "Event query failed.";
      return false;
    }
  }

  if (!render_context_.get()) {
    // Create the render context for extracting information from the events.
    render_context_ = CreateRenderContext();
    if (!render_context_.get())
      return false;
  }

  return true;
}

bool SystemSessionAnalyzer::Initialize() {
  DCHECK(!initialized_);

  // Fetch the first (current) session start event and the first session,
  // comprising an end and a start event for a total of 3 events.
  std::vector<EventInfo> events;
  if (!FetchEvents(3U, &events))
    return false;

  // Validate that the initial event is what we expect.
  if (events.size() != 3 || events[0].event_id != kIdSessionStart)
    return false;

  // Initialize the coverage start to allow detecting event time inversion.
  coverage_start_ = events[0].event_time;

  if (!ProcessSession(events[1], events[2]))
    return false;

  sessions_queried_ = 1;

  return true;
}

bool SystemSessionAnalyzer::ProcessSession(const EventInfo& end,
                                           const EventInfo& start) {
  // Validate the ordering of events (newest to oldest). The  expectation is a
  // (start / [unclean]shutdown) pair of events for each session.
  if (coverage_start_ < end.event_time)
    return false;
  if (end.event_time < start.event_time)
    return false;

  // Process a (start / shutdown) event pair, validating the types of events
  // and recording unclean sessions.
  if (start.event_id != kIdSessionStart)
    return false;  // Unexpected event type.
  if (end.event_id != kIdSessionEnd && end.event_id != kIdSessionEndUnclean)
    return false;  // Unexpected event type.

  if (end.event_id == kIdSessionEndUnclean) {
    unclean_sessions_.insert(
        std::make_pair(start.event_time, end.event_time - start.event_time));
  }

  coverage_start_ = start.event_time;

  return true;
}

SystemSessionAnalyzer::EvtHandle SystemSessionAnalyzer::CreateRenderContext() {
  LPCWSTR value_paths[] = {kEventIdPath, kEventTimePath};
  const DWORD kValueCnt = base::size(value_paths);

  EVT_HANDLE context = nullptr;
  context =
      ::EvtCreateRenderContext(kValueCnt, value_paths, EvtRenderContextValues);
  if (!context)
    DLOG(ERROR) << "Failed to create render context.";

  return EvtHandle(context);
}

}  // namespace metrics
