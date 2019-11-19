// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/extensions/accelerator_priority.h"

#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/ui/accelerator_utils.h"
#include "chrome/common/extensions/manifest_handlers/ui_overrides_handler.h"
#include "extensions/browser/extension_registry.h"
#include "ui/base/accelerators/accelerator.h"

ui::AcceleratorManager::HandlerPriority GetAcceleratorPriority(
    const ui::Accelerator& accelerator,
    const extensions::Extension* extension) {
  // Extensions overriding the bookmark shortcut need normal priority to
  // preserve the built-in processing order of the key and not override
  // WebContents key handling.
  if (accelerator == chrome::GetPrimaryChromeAcceleratorForBookmarkTab() &&
      extensions::UIOverrides::RemovesBookmarkShortcut(extension))
    return ui::AcceleratorManager::kNormalPriority;

  return ui::AcceleratorManager::kHighPriority;
}

ui::AcceleratorManager::HandlerPriority GetAcceleratorPriorityById(
    const ui::Accelerator& accelerator,
    const std::string& extension_id,
    content::BrowserContext* browser_context) {
  return GetAcceleratorPriority(
      accelerator,
      extensions::ExtensionRegistry::Get(browser_context)->enabled_extensions().
          GetByID(extension_id));
}
