// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/contents_layout_manager.h"

#include "ui/views/view.h"

ContentsLayoutManager::ContentsLayoutManager(views::View* devtools_view,
                                             views::View* contents_view)
    : devtools_view_(devtools_view),
      contents_view_(contents_view),
      host_(nullptr) {}

ContentsLayoutManager::~ContentsLayoutManager() {
}

void ContentsLayoutManager::SetContentsResizingStrategy(
    const DevToolsContentsResizingStrategy& strategy) {
  if (strategy_.Equals(strategy))
    return;

  strategy_.CopyFrom(strategy);
  if (host_)
    host_->InvalidateLayout();
}

void ContentsLayoutManager::Layout(views::View* contents_container) {
  DCHECK(host_ == contents_container);

  int height = contents_container->height();
  int width = contents_container->width();

  gfx::Size container_size(width, height);
  gfx::Rect new_devtools_bounds;
  gfx::Rect new_contents_bounds;

  ApplyDevToolsContentsResizingStrategy(strategy_, container_size,
      &new_devtools_bounds, &new_contents_bounds);

  // DevTools cares about the specific position, so we have to compensate RTL
  // layout here.
  devtools_view_->SetBoundsRect(host_->GetMirroredRect(new_devtools_bounds));
  contents_view_->SetBoundsRect(host_->GetMirroredRect(new_contents_bounds));
}

gfx::Size ContentsLayoutManager::GetPreferredSize(
    const views::View* host) const {
  return gfx::Size();
}

void ContentsLayoutManager::Installed(views::View* host) {
  DCHECK(!host_);
  host_ = host;
}
