// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_COMMON_EXTENSIONS_API_CAST_EXTENSION_MESSAGES_H_
#define CHROMECAST_COMMON_EXTENSIONS_API_CAST_EXTENSION_MESSAGES_H_

// Cast-specific IPC messages for extensions.
// Extension-related messages that aren't specific to Chromecast live in
// extensions/common/extension_messages.h.

#include <stdint.h>

#include <string>

#include "base/strings/string16.h"
#include "base/values.h"
#include "chromecast/common/extensions_api/automation_internal.h"
#include "extensions/common/stack_frame.h"
#include "ipc/ipc_message_macros.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_event.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/accessibility/ax_relative_bounds.h"
#include "ui/accessibility/ax_tree_data.h"
#include "ui/accessibility/ax_tree_update.h"
#include "ui/gfx/transform.h"
#include "url/gurl.h"

#define IPC_MESSAGE_START ChromeExtensionMsgStart

// Messages sent from the browser to the renderer.

IPC_STRUCT_TRAITS_BEGIN(ui::AXNodeData)
  IPC_STRUCT_TRAITS_MEMBER(id)
  IPC_STRUCT_TRAITS_MEMBER(role)
  IPC_STRUCT_TRAITS_MEMBER(state)
  IPC_STRUCT_TRAITS_MEMBER(actions)
  IPC_STRUCT_TRAITS_MEMBER(location)
  IPC_STRUCT_TRAITS_MEMBER(transform)
  IPC_STRUCT_TRAITS_MEMBER(string_attributes)
  IPC_STRUCT_TRAITS_MEMBER(int_attributes)
  IPC_STRUCT_TRAITS_MEMBER(float_attributes)
  IPC_STRUCT_TRAITS_MEMBER(bool_attributes)
  IPC_STRUCT_TRAITS_MEMBER(intlist_attributes)
  IPC_STRUCT_TRAITS_MEMBER(stringlist_attributes)
  IPC_STRUCT_TRAITS_MEMBER(html_attributes)
  IPC_STRUCT_TRAITS_MEMBER(child_ids)
  IPC_STRUCT_TRAITS_MEMBER(offset_container_id)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(ui::AXTreeData)
  IPC_STRUCT_TRAITS_MEMBER(tree_id)
  IPC_STRUCT_TRAITS_MEMBER(parent_tree_id)
  IPC_STRUCT_TRAITS_MEMBER(focused_tree_id)
  IPC_STRUCT_TRAITS_MEMBER(url)
  IPC_STRUCT_TRAITS_MEMBER(title)
  IPC_STRUCT_TRAITS_MEMBER(mimetype)
  IPC_STRUCT_TRAITS_MEMBER(doctype)
  IPC_STRUCT_TRAITS_MEMBER(loaded)
  IPC_STRUCT_TRAITS_MEMBER(loading_progress)
  IPC_STRUCT_TRAITS_MEMBER(focus_id)
  IPC_STRUCT_TRAITS_MEMBER(sel_anchor_object_id)
  IPC_STRUCT_TRAITS_MEMBER(sel_anchor_offset)
  IPC_STRUCT_TRAITS_MEMBER(sel_anchor_affinity)
  IPC_STRUCT_TRAITS_MEMBER(sel_focus_object_id)
  IPC_STRUCT_TRAITS_MEMBER(sel_focus_offset)
  IPC_STRUCT_TRAITS_MEMBER(sel_focus_affinity)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(ui::AXTreeUpdate)
  IPC_STRUCT_TRAITS_MEMBER(has_tree_data)
  IPC_STRUCT_TRAITS_MEMBER(tree_data)
  IPC_STRUCT_TRAITS_MEMBER(node_id_to_clear)
  IPC_STRUCT_TRAITS_MEMBER(root_id)
  IPC_STRUCT_TRAITS_MEMBER(nodes)
  IPC_STRUCT_TRAITS_MEMBER(event_from)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_BEGIN(ExtensionMsg_AccessibilityEventBundleParams)
  // ID of the accessibility tree that this event applies to.
  IPC_STRUCT_MEMBER(ui::AXTreeID, tree_id)

  // Zero or more updates to the accessibility tree to apply first.
  IPC_STRUCT_MEMBER(std::vector<ui::AXTreeUpdate>, updates)

  // Zero or more events to fire after the tree updates have been applied.
  IPC_STRUCT_MEMBER(std::vector<ui::AXEvent>, events)

  // The mouse location in screen coordinates.
  IPC_STRUCT_MEMBER(gfx::Point, mouse_location)
IPC_STRUCT_END()

IPC_STRUCT_BEGIN(ExtensionMsg_AccessibilityLocationChangeParams)
  // ID of the accessibility tree that this event applies to.
  IPC_STRUCT_MEMBER(ui::AXTreeID, tree_id)

  // ID of the object whose location is changing.
  IPC_STRUCT_MEMBER(int, id)

  // The object's new location info.
  IPC_STRUCT_MEMBER(ui::AXRelativeBounds, new_location)
IPC_STRUCT_END()

// Forward an accessibility message to an extension process where an
// extension is using the automation API to listen for accessibility events.
IPC_MESSAGE_CONTROL2(ExtensionMsg_AccessibilityEventBundle,
                     ExtensionMsg_AccessibilityEventBundleParams /* events */,
                     bool /* is_active_profile */)

// Forward an accessibility location change message to an extension process
// where an extension is using the automation API to listen for
// accessibility events.
IPC_MESSAGE_CONTROL1(ExtensionMsg_AccessibilityLocationChange,
                     ExtensionMsg_AccessibilityLocationChangeParams)

#endif  // CHROMECAST_COMMON_EXTENSIONS_API_CAST_EXTENSION_MESSAGES_H_
