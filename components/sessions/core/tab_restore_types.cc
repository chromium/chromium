// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sessions/core/tab_restore_types.h"

#include "base/trace_event/memory_usage_estimator.h"

namespace sessions::tab_restore {

TimeFactory::~TimeFactory() = default;

Entry::Entry(Type type)
    : id(SessionID::NewUnique()),
      original_id(SessionID::InvalidValue()),
      type(type) {}
Entry::~Entry() = default;

size_t Entry::EstimateMemoryUsage() const {
  return 0;
}

Tab::Tab() : Entry(TAB) {}
Tab::~Tab() = default;

size_t Tab::EstimateMemoryUsage() const {
  using base::trace_event::EstimateMemoryUsage;
  return EstimateMemoryUsage(navigations) +
         EstimateMemoryUsage(extension_app_id) +
         user_agent_override.EstimateMemoryUsage();
}

Window::Window() : Entry(WINDOW) {}
Window::~Window() = default;

size_t Window::EstimateMemoryUsage() const {
  using base::trace_event::EstimateMemoryUsage;
  return EstimateMemoryUsage(tabs) + EstimateMemoryUsage(app_name);
}

Group::Group() : Entry(GROUP) {}
Group::~Group() = default;

size_t Group::EstimateMemoryUsage() const {
  using base::trace_event::EstimateMemoryUsage;
  return EstimateMemoryUsage(tabs) + EstimateMemoryUsage(visual_data.title());
}

// static
std::unique_ptr<Group> Group::FromTab(const Tab& tab) {
  auto group = std::make_unique<Group>();
  CHECK(tab.group.has_value());
  group->group_id = tab.group.value();
  group->saved_group_id = tab.saved_group_id;
  group->visual_data = tab.group_visual_data.value();
  group->browser_id = tab.browser_id;
  group->timestamp = tab.timestamp;
  return group;
}

PlatformSpecificTabData::~PlatformSpecificTabData() = default;

}  // namespace sessions::tab_restore
