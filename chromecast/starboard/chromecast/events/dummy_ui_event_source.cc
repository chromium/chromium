// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// An implementation of UiEventSource::Create to be linked in for builds
// that do not use starboard.

#include <memory>

#include "chromecast/starboard/chromecast/events/ui_event_source.h"

namespace chromecast {

namespace {

// A dummy impl that reports that all events should be dispatched.
class DummyUiEventSource : public UiEventSource {
 public:
  DummyUiEventSource() = default;
  ~DummyUiEventSource() override = default;

  bool ShouldDispatchEvent(const ui::Event& /*event*/) override { return true; }
};

}  // namespace

// Declared in starboard_event_source.h.
std::unique_ptr<UiEventSource> UiEventSource::Create(
    ui::PlatformWindowDelegate* delegate) {
  return std::make_unique<DummyUiEventSource>();
}

}  // namespace chromecast
