// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/side_panel/extensions/extension_side_panel_action_item_util.h"

#include <optional>
#include <type_traits>

#include "base/check_op.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/browser_actions.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/side_panel/side_panel_action_callback.h"
#include "chrome/browser/ui/side_panel/side_panel_entry.h"
#include "extensions/common/extension.h"
#include "ui/actions/actions.h"
#include "ui/base/class_properties.h"

namespace extensions::side_panel_action_item_util {

namespace {

// Number of registered SidePanelEntries referencing an extension's shared side
// panel action item. The action item is removed when this reaches zero.
DEFINE_UI_CLASS_PROPERTY_KEY(int, kReferenceCountKey, 0)

SidePanelEntry::Key GetEntryKey(const ExtensionId& extension_id) {
  return SidePanelEntry::Key(SidePanelEntry::Id::kExtension, extension_id);
}

actions::ActionItem* FindActionItem(actions::ActionItem* root_action_item,
                                    const ExtensionId& extension_id) {
  std::optional<actions::ActionId> action_id =
      actions::ActionIdMap::StringToActionId(
          GetEntryKey(extension_id).ToString());
  if (!action_id.has_value()) {
    return nullptr;
  }
  return actions::ActionManager::Get().FindAction(action_id.value(),
                                                  root_action_item);
}

}  // namespace

void AcquireActionItem(BrowserWindowInterface* browser,
                       const Extension& extension) {
  // A reference is only acquired while registering an entry on a live window,
  // so the window's action tree is always present here.
  BrowserActions* browser_actions = BrowserActions::From(browser);
  CHECK(browser_actions);
  actions::ActionItem* root_action_item = browser_actions->root_action_item();
  actions::ActionItem* action_item =
      FindActionItem(root_action_item, extension.id());
  if (!action_item) {
    const SidePanelEntry::Key key = GetEntryKey(extension.id());
    const actions::ActionId action_id =
        actions::ActionIdMap::CreateActionId(key.ToString()).first;
    action_item = root_action_item->AddChild(
        actions::ActionItem::Builder(
            CreateToggleSidePanelActionCallback(key, browser))
            .SetText(base::UTF8ToUTF16(extension.short_name()))
            .SetActionId(action_id)
            .SetProperty(actions::kActionItemPinnableKey,
                         std::underlying_type_t<actions::ActionPinnableState>(
                             actions::ActionPinnableState::kPinnable))
            .Build());
  }
  action_item->SetProperty(kReferenceCountKey,
                           action_item->GetProperty(kReferenceCountKey) + 1);
}

void ReleaseActionItem(BrowserWindowInterface* browser,
                       const ExtensionId& extension_id) {
  BrowserActions* browser_actions = BrowserActions::From(browser);
  actions::ActionItem* root_action_item =
      browser_actions ? browser_actions->root_action_item() : nullptr;
  if (!root_action_item) {
    // The window's action tree has already been destroyed, which only happens
    // while the window is tearing down. The action item went away with it, so
    // there is nothing to release.
    return;
  }

  // A held reference keeps the action item alive, so it must still exist as
  // long as the action tree does.
  actions::ActionItem* action_item =
      FindActionItem(root_action_item, extension_id);
  CHECK(action_item);

  const int reference_count = action_item->GetProperty(kReferenceCountKey);
  CHECK_GT(reference_count, 0);
  if (reference_count > 1) {
    action_item->SetProperty(kReferenceCountKey, reference_count - 1);
    return;
  }
  root_action_item->RemoveChild(action_item).reset();
}

}  // namespace extensions::side_panel_action_item_util
