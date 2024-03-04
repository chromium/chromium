// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/editor_menu/editor_manager_factory.h"

#include <memory>

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chrome/browser/ui/views/editor_menu/editor_manager_lacros.h"
#else
#include "chrome/browser/ui/views/editor_menu/editor_manager_ash.h"
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

namespace chromeos::editor_menu {

std::unique_ptr<EditorManager> CreateEditorManager(
    content::BrowserContext* context) {
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  return std::make_unique<EditorManagerLacros>();
#else
  return std::make_unique<EditorManagerAsh>(context);
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)
}

}  // namespace chromeos::editor_menu
