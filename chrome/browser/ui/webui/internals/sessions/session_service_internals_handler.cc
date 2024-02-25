// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/internals/sessions/session_service_internals_handler.h"

#include <string>

#include "base/i18n/time_formatting.h"
#include "base/memory/ref_counted_memory.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/time/time.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sessions/session_service_log.h"
#include "chrome/common/url_constants.h"
#include "chrome/common/webui_url_constants.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_ui_data_source.h"

namespace {

// This is for debugging, so it doesn't use local time conversions.
std::string EventTimeToString(const SessionServiceEvent& event) {
  return base::UnlocalizedTimeFormatWithPattern(event.time, "M/d/y H:mm:ss");
}

std::string EventToString(const SessionServiceEvent& event) {
  switch (event.type) {
    case SessionServiceEventLogType::kStart:
      return base::StrCat(
          {EventTimeToString(event), " start",
           (event.data.start.did_last_session_crash ? " (last session crashed)"
                                                    : std::string())});
    case SessionServiceEventLogType::kRestore:
      return base::StrCat(
          {EventTimeToString(event), " restore windows=",
           base::NumberToString(event.data.restore.window_count),
           " tabs=", base::NumberToString(event.data.restore.tab_count),
           (event.data.restore.encountered_error_reading ? " (error reading)"
                                                         : std::string())});
    case SessionServiceEventLogType::kExit:
      return base::StrCat(
          {EventTimeToString(event), " exit (shutdown) windows=",
           base::NumberToString(event.data.exit.window_count),
           " tabs=", base::NumberToString(event.data.exit.tab_count),
           " is_first_service=",
           base::NumberToString(event.data.exit.is_first_session_service),
           " did_schedule_command=",
           base::NumberToString(event.data.exit.did_schedule_command)});

    case SessionServiceEventLogType::kWriteError:
      return base::StrCat(
          {EventTimeToString(event), " write errors (",
           base::NumberToString(event.data.write_error.error_count), ")"});

    case SessionServiceEventLogType::kRestoreCanceled:
      return base::StrCat({EventTimeToString(event), " restore canceled"});

    case SessionServiceEventLogType::kRestoreInitiated:
      return base::StrCat(
          {EventTimeToString(event), " restore initiated sync=",
           base::NumberToString(event.data.restore_initiated.synchronous),
           " restore_browser=",
           base::NumberToString(event.data.restore_initiated.restore_browser)});
  }
}

std::string GetEventLogAsString(Profile* profile) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  std::vector<std::string> results;
  results.push_back("<pre>");
  for (const auto& event : GetSessionServiceEvents(profile))
    results.push_back(EventToString(event));
  results.push_back("</pre>");
  return base::JoinString(results, "\n");
}

}  // namespace

// static
bool SessionServiceInternalsHandler::ShouldHandleWebUIRequestCallback(
    const std::string& path) {
  return path == chrome::kChromeUISessionServiceInternalsPath;
}

// static
void SessionServiceInternalsHandler::HandleWebUIRequestCallback(
    Profile* profile,
    const std::string& path,
    content::WebUIDataSource::GotDataCallback callback) {
  DCHECK(ShouldHandleWebUIRequestCallback(path));
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  std::move(callback).Run(base::MakeRefCounted<base::RefCountedString>(
      GetEventLogAsString(profile)));
}
