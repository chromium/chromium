// Copyright 2021 The Chromium Authors
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
    : cg_window_id_(cg_window_id),
      frame_sink_id_(frame_sink_id),
      weak_factory_(this) {
  DCHECK_EQ(GetMap().count(cg_window_id), 0u);
  GetMap()[cg_window_id] = this;
}

ScopedCGWindowID::~ScopedCGWindowID() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  weak_factory_.InvalidateWeakPtrs();

  auto found = GetMap().find(cg_window_id_);
  DCHECK_EQ(found->second, this);
  GetMap().erase(found);

  for (auto& observer : observer_list_)
    observer.OnScopedCGWindowIDDestroyed(cg_window_id_);
  observer_list_.Clear();
}

void ScopedCGWindowID::AddObserver(Observer* observer) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  observer_list_.AddObserver(observer);
}

void ScopedCGWindowID::RemoveObserver(Observer* observer) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  observer_list_.RemoveObserver(observer);
}

void ScopedCGWindowID::OnMouseMoved(const gfx::PointF& location_in_window,
                                    const gfx::Size& window_size) {
  for (auto& observer : observer_list_) {
    observer.OnScopedCGWindowIDMouseMoved(cg_window_id_, location_in_window,
                                          window_size);
  }
}

// static
base::WeakPtr<ScopedCGWindowID> ScopedCGWindowID::Get(uint32_t cg_window_id) {
  auto found = GetMap().find(cg_window_id);
  if (found == GetMap().end())
    return nullptr;
  DCHECK_CALLED_ON_VALID_THREAD(found->second->thread_checker_);

  return found->second->weak_factory_.GetWeakPtr();
}

}  // namespace remote_cocoa
