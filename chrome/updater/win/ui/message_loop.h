// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_WIN_UI_MESSAGE_LOOP_H_
#define CHROME_UPDATER_WIN_UI_MESSAGE_LOOP_H_

#include <windows.h>

#include <vector>

namespace updater::ui {

// A message filter is given the chance to handle every Win32 message pulled
// off the queue before it is translated and dispatched. Returning `TRUE`
// indicates the message was consumed.
class MessageFilter {
 public:
  virtual ~MessageFilter() = default;
  virtual BOOL PreTranslateMessage(MSG* msg) = 0;
};

// A simple replacement for `WTL::CMessageLoop` used by the updater UI.
//
// Filters are dispatched in reverse registration order, matching WTL's
// behavior so the most-recently-added filter (typically the top-most dialog)
// gets first crack at a message.
class MessageLoop {
 public:
  MessageLoop();
  MessageLoop(const MessageLoop&) = delete;
  MessageLoop& operator=(const MessageLoop&) = delete;
  ~MessageLoop();

  // Registers a filter. The pointed-to object must outlive the registration.
  void AddMessageFilter(MessageFilter* filter);

  // Removes a previously-registered filter. Safe to call with an unregistered
  // filter (no-op).
  void RemoveMessageFilter(MessageFilter* filter);

  // Pumps messages until WM_QUIT is received. Returns the WM_QUIT wParam.
  int Run();

 private:
  // Calls each registered filter (back-to-front) and returns true if any of
  // them claimed the message.
  bool RunFilters(MSG* msg);

  std::vector<MessageFilter*> filters_;

  // Non-zero while `RunFilters` is iterating `filters_`. When non-zero,
  // `RemoveMessageFilter` nulls slots in place instead of erasing them so
  // that a filter destroyed mid-cycle cannot be dereferenced on a
  // subsequent iteration. The vector is compacted when the count returns
  // to zero. An int (not a bool) is used to tolerate reentrancy.
  int in_run_filters_ = 0;
};

}  // namespace updater::ui

#endif  // CHROME_UPDATER_WIN_UI_MESSAGE_LOOP_H_
