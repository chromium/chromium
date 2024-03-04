// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_EDITOR_MENU_EDITOR_MANAGER_FACTORY_H_
#define CHROME_BROWSER_UI_VIEWS_EDITOR_MENU_EDITOR_MANAGER_FACTORY_H_

#include <memory>

#include "chrome/browser/ui/views/editor_menu/editor_manager.h"
#include "content/public/browser/browser_context.h"

namespace chromeos::editor_menu {

std::unique_ptr<EditorManager> CreateEditorManager(
    content::BrowserContext* context);

}  // namespace chromeos::editor_menu

#endif  // CHROME_BROWSER_UI_VIEWS_EDITOR_MENU_EDITOR_MANAGER_FACTORY_H_
