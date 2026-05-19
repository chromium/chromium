// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/win/ui/message_loop.h"

#include <windows.h>

namespace updater::ui {

MessageLoop::MessageLoop() = default;
MessageLoop::~MessageLoop() = default;

void MessageLoop::AddMessageFilter(MessageFilter* filter) {
  if (!filter) {
    return;
  }
  filters_.push_back(filter);
}

void MessageLoop::RemoveMessageFilter(MessageFilter* filter) {
  // If a filter is being removed while `RunFilters` is iterating (e.g. the
  // filter is destroying itself synchronously from inside its own
  // `PreTranslateMessage` or from a window-procedure call dispatched by
  // another filter), erasing here would both invalidate the iterator and
  // leave a dangling pointer in any in-flight snapshot. Replace the entry
  // with `nullptr` so the current iteration skips it; the null slots are
  // compacted in `RunFilters` once iteration completes.
  if (in_run_filters_ > 0) {
    for (auto& slot : filters_) {
      if (slot == filter) {
        slot = nullptr;
      }
    }
    return;
  }
  std::erase(filters_, filter);
}

bool MessageLoop::RunFilters(MSG* msg) {
  // Iterate `filters_` directly (in reverse registration order so the
  // most-recently-installed filter sees the message first, matching WTL).
  // `RemoveMessageFilter` cooperates while `in_run_filters_` is non-zero
  // by nulling entries instead of erasing, which keeps indices stable and
  // ensures we never dereference a filter that was destroyed mid-cycle.
  ++in_run_filters_;
  bool handled = false;
  // Snapshot the size at entry so any filter appended during dispatch is
  // deferred to a subsequent message rather than receiving the current one
  // out of registration order.
  const size_t count = filters_.size();
  for (size_t i = count; i > 0; --i) {
    MessageFilter* filter = filters_[i - 1];
    if (filter && filter->PreTranslateMessage(msg)) {
      handled = true;
      break;
    }
  }
  if (--in_run_filters_ == 0) {
    std::erase(filters_, nullptr);
  }
  return handled;
}

int MessageLoop::Run() {
  MSG msg = {};
  while (::GetMessageW(&msg, nullptr, 0, 0) > 0) {
    if (!RunFilters(&msg)) {
      ::TranslateMessage(&msg);
      ::DispatchMessageW(&msg);
    }
  }
  return static_cast<int>(msg.wParam);
}

}  // namespace updater::ui
