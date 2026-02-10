// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browser_apis/tab_strip/types/path.h"

#include <utility>

namespace tabs_api {

Path::Path() = default;
Path::Path(std::vector<tabs_api::NodeId> components)
    : components_(std::move(components)) {}
Path::~Path() = default;

Path::Path(const Path&) = default;
Path::Path(Path&&) noexcept = default;
Path& Path::operator=(const Path&) = default;
Path& Path::operator=(Path&&) noexcept = default;

bool operator==(const Path& a, const Path& b) {
  return a.components_ == b.components_;
}

}  // namespace tabs_api
