// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/remote_cocoa/browser/scoped_cg_window_id.h"

#include <map>

#include "base/no_destructor.h"

namespace remote_cocoa {

namespace {

using ScoperMap = std::map<uint32_t, ScopedCGWindowID*>;

ScoperMap& GetMap() {
  static base::NoDestructor<ScoperMap> map;
  return *map.get();
}

}  // namespace

ScopedCGWindowID::ScopedCGWindowID(uint32_t cg_window_id,
                                   const viz::FrameSinkId& frame_sink_id)
    : cg_window_id_(cg_window_id), frame_sink_id_(frame_sink_id) {
  DCHECK_EQ(GetMap().count(cg_window_id), 0u);
  GetMap()[cg_window_id] = this;
}

ScopedCGWindowID::~ScopedCGWindowID() {
  auto found = GetMap().find(cg_window_id_);
  DCHECK_EQ(found->second, this);
  GetMap().erase(found);
}

// static
ScopedCGWindowID* ScopedCGWindowID::Get(uint32_t cg_window_id) {
  auto found = GetMap().find(cg_window_id);
  if (found == GetMap().end())
    return nullptr;
  return found->second;
}

}  // namespace remote_cocoa
