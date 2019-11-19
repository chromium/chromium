// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/browser/ui/aura/accessibility/ax_tree_source_aura.h"

#include "chromecast/browser/accessibility/accessibility_manager.h"
#include "chromecast/browser/cast_browser_process.h"
#include "chromecast/browser/ui/aura/accessibility/automation_manager_aura.h"
#include "ui/accessibility/ax_action_data.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/accessibility/ax_tree_data.h"
#include "ui/aura/client/focus_client.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#include "ui/views/accessibility/ax_aura_obj_wrapper.h"

AXTreeSourceAura::AXTreeSourceAura(views::AXAuraObjWrapper* root,
                                   const ui::AXTreeID& tree_id,
                                   views::AXAuraObjCache* cache)
    : AXTreeSourceViews(root, tree_id, cache) {}

AXTreeSourceAura::~AXTreeSourceAura() = default;

bool AXTreeSourceAura::GetTreeData(ui::AXTreeData* tree_data) const {
  AXTreeSourceViews::GetTreeData(tree_data);

  aura::Window* root_window =
      chromecast::shell::CastBrowserProcess::GetInstance()
          ->accessibility_manager()
          ->window_tree_host()
          ->window();
  if (root_window) {
    aura::client::FocusClient* focus_client =
        aura::client::GetFocusClient(root_window);
    if (focus_client) {
      aura::Window* window = focus_client->GetFocusedWindow();
      tree_data->focus_id =
          AutomationManagerAura::GetInstance()->GetIDFromWindow(window);
    }
  }
  return true;
}

void AXTreeSourceAura::SerializeNode(views::AXAuraObjWrapper* node,
                                     ui::AXNodeData* out_data) const {
  AXTreeSourceViews::SerializeNode(node, out_data);

  if (out_data->role == ax::mojom::Role::kWebView) {
    // TODO(rmrossi) : Figure out whether this will ever be required
    // for chromecast.
    LOG(FATAL) << "Unhandled role";
  }
}
