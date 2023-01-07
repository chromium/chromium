// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/renderer_context_menu/render_view_context_menu_observer.h"

bool RenderViewContextMenuObserver::IsCommandIdSupported(int command_id) {
  return false;
}

bool RenderViewContextMenuObserver::IsCommandIdChecked(int command_id) {
  return false;
}

bool RenderViewContextMenuObserver::IsCommandIdEnabled(int command_id) {
  return false;
}
