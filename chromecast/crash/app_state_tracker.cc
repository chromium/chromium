// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/crash/app_state_tracker.h"

#include "base/no_destructor.h"
#include "chromecast/crash/cast_crash_storage.h"

namespace {

struct CurrentAppState {
  std::string previous_app;
  std::string current_app;
  std::string last_launched_app;
  std::string stadia_session_id;
};

CurrentAppState* GetAppState() {
  static base::NoDestructor<CurrentAppState> app_state;
  return app_state.get();
}

}  // namespace

namespace chromecast {

// static
std::string AppStateTracker::GetLastLaunchedApp() {
  return GetAppState()->last_launched_app;
}

// static
std::string AppStateTracker::GetCurrentApp() {
  return GetAppState()->current_app;
}

// static
std::string AppStateTracker::GetPreviousApp() {
  return GetAppState()->previous_app;
}

// static
std::string AppStateTracker::GetStadiaSessionId() {
  return GetAppState()->stadia_session_id;
}

// static
void AppStateTracker::SetLastLaunchedApp(const std::string& app_id) {
  GetAppState()->last_launched_app = app_id;
  CastCrashStorage::GetInstance()->SetLastLaunchedApp(app_id);
}

// static
void AppStateTracker::SetCurrentApp(const std::string& app_id) {
  CurrentAppState* app_state = GetAppState();
  app_state->previous_app = app_state->current_app;
  app_state->current_app = app_id;

  CastCrashStorage::GetInstance()->SetCurrentApp(app_id);
  CastCrashStorage::GetInstance()->SetPreviousApp(app_state->previous_app);
}

// static
void AppStateTracker::SetPreviousApp(const std::string& app_id) {
  GetAppState()->previous_app = app_id;
  CastCrashStorage::GetInstance()->SetPreviousApp(app_id);
}

// static
void AppStateTracker::SetStadiaSessionId(const std::string& stadia_session_id) {
  if (!stadia_session_id.empty()) {
    GetAppState()->stadia_session_id = stadia_session_id;
    CastCrashStorage::GetInstance()->SetStadiaSessionId(stadia_session_id);
  } else {
    GetAppState()->stadia_session_id.clear();
    CastCrashStorage::GetInstance()->ClearStadiaSessionId();
  }
}

}  // namespace chromecast
