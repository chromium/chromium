// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sessions/core/tab_restore_service.h"

#include "base/trace_event/memory_usage_estimator.h"
#include "components/tab_groups/tab_group_id.h"
#include "components/tab_groups/tab_group_visual_data.h"

namespace sessions {

// TimeFactory-----------------------------------------------------------------

TabRestoreService::TimeFactory::~TimeFactory() {}

// Entry ----------------------------------------------------------------------

TabRestoreService::Entry::~Entry() = default;

TabRestoreService::Entry::Entry(Type type)
    : id(SessionID::NewUnique()),
      original_id(SessionID::InvalidValue()),
      type(type) {}

size_t TabRestoreService::Entry::EstimateMemoryUsage() const {
  return 0;
}

TabRestoreService::Tab::Tab() : Entry(TAB) {}
TabRestoreService::Tab::~Tab() = default;

size_t TabRestoreService::Tab::EstimateMemoryUsage() const {
  using base::trace_event::EstimateMemoryUsage;
  return EstimateMemoryUsage(navigations) +
         EstimateMemoryUsage(extension_app_id) +
         user_agent_override.EstimateMemoryUsage();
}

TabRestoreService::Window::Window() : Entry(WINDOW) {}
TabRestoreService::Window::~Window() = default;

size_t TabRestoreService::Window::EstimateMemoryUsage() const {
  using base::trace_event::EstimateMemoryUsage;
  return
      EstimateMemoryUsage(tabs) +
      EstimateMemoryUsage(app_name);
}

TabRestoreService::Group::Group() : Entry(GROUP) {}
TabRestoreService::Group::~Group() = default;

size_t TabRestoreService::Group::EstimateMemoryUsage() const {
  using base::trace_event::EstimateMemoryUsage;
  return EstimateMemoryUsage(tabs) + EstimateMemoryUsage(visual_data.title());
}

// TabRestoreService ----------------------------------------------------------

TabRestoreService::~TabRestoreService() {
}

// PlatformSpecificTabData
// ------------------------------------------------------

PlatformSpecificTabData::~PlatformSpecificTabData() {}

}  // namespace sessions
