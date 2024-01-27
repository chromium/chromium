// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/breadcrumbs/core/breadcrumb_manager_browser_agent.h"

#include <string>
#include <vector>

#include "base/format_macros.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"

namespace breadcrumbs {

BreadcrumbManagerBrowserAgent::BreadcrumbManagerBrowserAgent() {
  static int next_unique_id = 1;
  unique_id_ = next_unique_id++;
}

bool BreadcrumbManagerBrowserAgent::IsLoggingEnabled() {
  return is_logging_enabled_;
}

void BreadcrumbManagerBrowserAgent::SetLoggingEnabled(bool enabled) {
  is_logging_enabled_ = enabled;
}

void BreadcrumbManagerBrowserAgent::LogEvent(const std::string& event) {
  if (!IsLoggingEnabled())
    return;
  PlatformLogEvent(
      base::StringPrintf("Browser%d %s", unique_id_, event.c_str()));
}

void BreadcrumbManagerBrowserAgent::LogTabsInserted(int num_tabs) {
  LogEvent(base::StringPrintf("Inserted %d tabs", num_tabs));
}

void BreadcrumbManagerBrowserAgent::LogTabInsertedAt(int tab_id,
                                                     int index,
                                                     bool is_tab_active) {
  const char* activating_string = is_tab_active ? "active" : "inactive";
  LogEvent(base::StringPrintf("Insert %s Tab%d at %d", activating_string,
                              tab_id, index));
}

void BreadcrumbManagerBrowserAgent::LogTabsClosed(int num_tabs) {
  LogEvent(base::StringPrintf("Closed %d tabs", num_tabs));
}

void BreadcrumbManagerBrowserAgent::LogTabClosedAt(int tab_id, int index) {
  LogEvent(base::StringPrintf("Close Tab%d at %d", tab_id, index));
}

void BreadcrumbManagerBrowserAgent::LogTabMoved(int tab_id,
                                                int from_index,
                                                int to_index) {
  LogEvent(base::StringPrintf("Moved Tab%d from %d to %d", tab_id, from_index,
                              to_index));
}

void BreadcrumbManagerBrowserAgent::LogTabReplaced(int old_tab_id,
                                                   int new_tab_id,
                                                   int index) {
  LogEvent(base::StringPrintf("Replaced Tab%d with Tab%d at %d", old_tab_id,
                              new_tab_id, index));
}

void BreadcrumbManagerBrowserAgent::LogActiveTabChanged(
    std::optional<int> old_tab_id,
    std::optional<int> new_tab_id,
    std::optional<size_t> index) {
  std::vector<std::string> event = {"Switch"};
  if (old_tab_id) {
    event.push_back(base::StringPrintf("from Tab%d", old_tab_id.value()));
  }
  if (new_tab_id) {
    DCHECK(index.has_value());
    event.push_back(base::StringPrintf("to Tab%d at %" PRIuS,
                                       new_tab_id.value(), index.value()));
  }
  LogEvent(base::JoinString(event, " "));
}

}  // namespace breadcrumbs
