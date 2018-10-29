// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/browser/ui/aura/accessibility/ax_tree_source_aura.h"

#include <stddef.h>

#include "chromecast/browser/ui/aura/accessibility/automation_manager_aura.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "ui/accessibility/ax_action_data.h"
#include "ui/views/accessibility/ax_aura_obj_wrapper.h"
#include "ui/views/accessibility/ax_view_obj_wrapper.h"

AXTreeSourceAura::AXTreeSourceAura()
    : desktop_root_(std::make_unique<AXRootObjWrapper>(
          AutomationManagerAura::GetInstance())) {}

AXTreeSourceAura::~AXTreeSourceAura() = default;

bool AXTreeSourceAura::GetTreeData(ui::AXTreeData* tree_data) const {
  tree_data->tree_id = ui::DesktopAXTreeID();
  AXTreeSourceViews::GetTreeData(tree_data);

  // TODO(b/111911092): AXTreeData::focus_id represents the node within the
  // tree with 'keyboard focus'.  We have no keyboard focus on chromecast so
  // this is being left as -1. This prevents getFocus calls from the chromevox
  // background page from finding any window in focus and interferes with
  // gesture event processing.  Since we only ever have one top level window
  // and one ax tree, temporarily returning 1 here to indicate the root node
  // is always the focused window. A better solution would be to fix the focus
  // issues on chromecast which relies on a) the root window to be focused via
  // Focus() and 2) a native widget being registered with the root window so
  // the above GetFocus call will work.  When this code is re-unified with
  // chrome, this will need to be a special case for chromecast unless the
  // better solution described above is implemented.
  tree_data->focus_id = 1;
  return true;
}

views::AXAuraObjWrapper* AXTreeSourceAura::GetRoot() const {
  return desktop_root_.get();
}

void AXTreeSourceAura::SerializeNode(views::AXAuraObjWrapper* node,
                                     ui::AXNodeData* out_data) const {
  AXTreeSourceViews::SerializeNode(node, out_data);

  if (out_data->role == ax::mojom::Role::kWebView) {
    // TODO(rmrossi) : Figure out whether this will ever be required
    // for chromecast.
    LOG(FATAL) << "Unhandled role";
  } else if (out_data->role == ax::mojom::Role::kWindow ||
             out_data->role == ax::mojom::Role::kDialog) {
    // Add clips children flag by default to these roles.
    out_data->AddBoolAttribute(ax::mojom::BoolAttribute::kClipsChildren, true);
  }
}

