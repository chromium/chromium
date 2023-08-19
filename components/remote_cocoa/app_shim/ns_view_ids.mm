// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/remote_cocoa/app_shim/ns_view_ids.h"

#import <Cocoa/Cocoa.h>

#include <map>
#include <utility>

#include "base/check.h"
#include "base/no_destructor.h"

namespace remote_cocoa {

std::map<uint64_t, NSView*>& GetIdToNSViewMap() {
  static base::NoDestructor<std::map<uint64_t, NSView*>> instance;
  return *instance;
}

std::map<NSView*, uint64_t>& GetNSViewToIdMap() {
  static base::NoDestructor<std::map<NSView*, uint64_t>> instance;
  return *instance;
}

NSView* GetNSViewFromId(uint64_t ns_view_id) {
  auto& id_to_view_map = GetIdToNSViewMap();
  auto found = id_to_view_map.find(ns_view_id);
  if (found == id_to_view_map.end())
    return nil;
  return found->second;
}

uint64_t GetIdFromNSView(NSView* ns_view) {
  auto& view_to_id_map = GetNSViewToIdMap();
  auto found = view_to_id_map.find(ns_view);
  if (found == view_to_id_map.end())
    return 0;
  return found->second;
}

ScopedNSViewIdMapping::ScopedNSViewIdMapping(uint64_t ns_view_id, NSView* view)
    : ns_view_(view), ns_view_id_(ns_view_id) {
  DCHECK(ns_view_id_);
  {
    auto result = GetIdToNSViewMap().insert(std::make_pair(ns_view_id, view));
    DCHECK(result.second);
  }
  {
    auto result = GetNSViewToIdMap().insert(std::make_pair(view, ns_view_id));
    DCHECK(result.second);
  }
}

ScopedNSViewIdMapping::~ScopedNSViewIdMapping() {
  {
    auto found = GetIdToNSViewMap().find(ns_view_id_);
    DCHECK(found != GetIdToNSViewMap().end());
    GetIdToNSViewMap().erase(found);
  }
  {
    auto found = GetNSViewToIdMap().find(ns_view_);
    DCHECK(found != GetNSViewToIdMap().end());
    GetNSViewToIdMap().erase(found);
  }
}

}  // namespace remote_cocoa
