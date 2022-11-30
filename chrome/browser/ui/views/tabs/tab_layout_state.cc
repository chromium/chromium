// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/tab_layout_state.h"

#include "base/notreached.h"
#include "chrome/browser/ui/tabs/tab_types.h"
#include "chrome/browser/ui/views/tabs/tab_strip_layout_types.h"
#include "ui/gfx/animation/tween.h"

TabLayoutState TabLayoutState::WithOpen(TabOpen open) const {
  return TabLayoutState(open, pinnedness_, activeness_);
}

TabLayoutState TabLayoutState::WithPinned(TabPinned pinned) const {
  return TabLayoutState(openness_, pinned, activeness_);
}

TabLayoutState TabLayoutState::WithActive(TabActive active) const {
  return TabLayoutState(openness_, pinnedness_, active);
}

bool TabLayoutState::IsClosed() const {
  return openness_ == TabOpen::kClosed;
}
