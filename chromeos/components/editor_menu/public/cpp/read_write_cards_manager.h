// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_EDITOR_MENU_PUBLIC_CPP_READ_WRITE_CARDS_MANAGER_H_
#define CHROMEOS_COMPONENTS_EDITOR_MENU_PUBLIC_CPP_READ_WRITE_CARDS_MANAGER_H_

#include <vector>

#include "base/component_export.h"
#include "base/functional/callback.h"

namespace content {
class BrowserContext;
struct ContextMenuParams;
}  // namespace content

namespace gfx {
class Rect;
}  // namespace gfx

namespace chromeos {

class ReadWriteCardController;

namespace editor_menu {
using FetchControllersCallback = base::OnceCallback<void(
    std::vector<base::WeakPtr<ReadWriteCardController>>)>;
}

// A manager to manage the controllers of Quick Answers, Editor Menu, or Mahi
// Menu.
class COMPONENT_EXPORT(EDITOR_MENU_PUBLIC_CPP) ReadWriteCardsManager {
 public:
  ReadWriteCardsManager();
  virtual ~ReadWriteCardsManager();

  static ReadWriteCardsManager* Get();

  virtual void FetchController(
      const content::ContextMenuParams& params,
      content::BrowserContext* context,
      editor_menu::FetchControllersCallback callback) = 0;

  virtual void SetContextMenuBounds(const gfx::Rect& context_menu_bounds) = 0;
};

}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_EDITOR_MENU_PUBLIC_CPP_READ_WRITE_CARDS_MANAGER_H_
