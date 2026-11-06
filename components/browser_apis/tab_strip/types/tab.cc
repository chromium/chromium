// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browser_apis/tab_strip/types/tab.h"

namespace tabs_api {

Tab::Tab() = default;
Tab::Tab(const Tab&) = default;
Tab::Tab(Tab&&) noexcept = default;
Tab& Tab::operator=(const Tab&) = default;
Tab& Tab::operator=(Tab&&) noexcept = default;
Tab::~Tab() = default;

tabs::SupportsTabHandles::Handle Tab::ToTabHandle() const {
  return id.ToTabHandle().value_or(tabs::SupportsTabHandles::Handle::Null());
}

}  // namespace tabs_api
