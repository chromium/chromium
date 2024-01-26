// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BREADCRUMBS_CORE_BREADCRUMB_MANAGER_BROWSER_AGENT_H_
#define COMPONENTS_BREADCRUMBS_CORE_BREADCRUMB_MANAGER_BROWSER_AGENT_H_

#include <optional>
#include <string>

namespace breadcrumbs {

// Logs activity for the associated Browser's underlying list of tabs based on
// callbacks from various observers. Event logs are sent to the
// BreadcrumbManagerKeyedService for the BrowserState (iOS) or BrowserContext
// (desktop). For example:
//   Browser1 Insert active Tab2 at 0
// which indicates that a tab with identifier 2 (from
// BreadcrumbManagerTabHelper) was inserted into the Browser with identifier 1
// (from BreadcrumbManagerBrowserAgent) at index 0.
class BreadcrumbManagerBrowserAgent {
 public:
  BreadcrumbManagerBrowserAgent();

  // Gets and sets whether or not logging is enabled. Disabling logging is used
  // to prevent the over-collection of breadcrumb events during known states
  // such as a clean shutdown.
  // `IsLoggingEnabled()` defaults to true on initialization.
  bool IsLoggingEnabled();
  void SetLoggingEnabled(bool enabled);

 protected:
  // Logs the breadcrumb event for inserting `num_tabs` tabs.
  void LogTabsInserted(int num_tabs);

  // Logs the breadcrumb event for inserting the tab identified by `tab_id` at
  // position `index`, including whether the tab is currently active per
  // `is_tab_active`.
  void LogTabInsertedAt(int tab_id, int index, bool is_tab_active);

  // Logs the breadcrumb event for closing `num_tabs` tabs.
  void LogTabsClosed(int num_tabs);

  // Logs the breadcrumb event for closing the tab identified by `tab_id` at
  // position `index`.
  void LogTabClosedAt(int tab_id, int index);

  // Logs the breadcrumb event for moving the tab identified by `tab_id` from
  // position `from_index` to position `to_index`.
  void LogTabMoved(int tab_id, int from_index, int to_index);

  // Logs the breadcrumb event for replacing the tab identified by `old_tab_id`
  // with the tab identified by `new_tab_id` at position `index`.
  void LogTabReplaced(int old_tab_id, int new_tab_id, int index);

  // Logs the breadcrumb event for changing the active tab from the tab
  // identified by `old_tab_id` to the tab identified by `new_tab_id` at
  // position `index`.
  void LogActiveTabChanged(std::optional<int> old_tab_id,
                           std::optional<int> new_tab_id,
                           std::optional<size_t> index);

  // Logs a breadcrumb event with message data `event` for the associated
  // browser. NOTE: `event` must not include newline characters, as newlines are
  // used by BreadcrumbPersistentStorageManager as a deliminator.
  void LogEvent(const std::string& event);

  int unique_id() { return unique_id_; }

 private:
  // Logs the given `event` for the associated browser by retrieving the
  // breadcrumb manager from BrowserState (iOS) or BrowserContext (desktop).
  // This should not be used directly to log events; use LogEvent() instead.
  virtual void PlatformLogEvent(const std::string& event) = 0;

  // Whether or not events will be logged.
  bool is_logging_enabled_ = true;

  // Unique (across this application run only) identifier for logs associated
  // with the associated browser instance. Used to identify logs associated with
  // the same underlying BrowserState (iOS) or BrowserContext (desktop).
  int unique_id_ = -1;
};

}  // namespace breadcrumbs

#endif  // COMPONENTS_BREADCRUMBS_CORE_BREADCRUMB_MANAGER_BROWSER_AGENT_H_
