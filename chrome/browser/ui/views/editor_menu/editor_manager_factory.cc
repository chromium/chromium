// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/editor_menu/editor_manager_factory.h"

#include <memory>

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chrome/browser/ui/views/editor_menu/editor_manager_lacros.h"
#else
#include "chrome/browser/ash/input_method/editor_mediator.h"
#include "chrome/browser/ash/input_method/editor_mediator_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/views/editor_menu/editor_manager_ash.h"
#include "chromeos/constants/chromeos_features.h"
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

namespace chromeos::editor_menu {

std::unique_ptr<EditorManager> CreateEditorManager(
    content::BrowserContext* context) {
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  return std::make_unique<EditorManagerLacros>();
#else
  CHECK(chromeos::features::IsOrcaEnabled());
  ash::input_method::EditorMediator* editor_mediator =
      ash::input_method::EditorMediatorFactory::GetInstance()->GetForProfile(
          Profile::FromBrowserContext(context));

  return editor_mediator != nullptr ? std::make_unique<EditorManagerAsh>(
                                          editor_mediator->panel_manager())
                                    : nullptr;
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)
}

}  // namespace chromeos::editor_menu
