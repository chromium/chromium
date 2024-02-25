// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_EDITOR_MENU_PUBLIC_CPP_READ_WRITE_CARDS_MANAGER_H_
#define CHROMEOS_COMPONENTS_EDITOR_MENU_PUBLIC_CPP_READ_WRITE_CARDS_MANAGER_H_

#include "base/component_export.h"
#include "base/functional/callback.h"

namespace content {
class BrowserContext;
struct ContextMenuParams;
}  // namespace content

namespace chromeos {

class ReadWriteCardController;

namespace editor_menu {
using FetchControllerCallback =
    base::OnceCallback<void(base::WeakPtr<ReadWriteCardController>)>;
}

// A manager to manage the controllers of Quick Answers or Editor Menu.
class COMPONENT_EXPORT(EDITOR_MENU_PUBLIC_CPP) ReadWriteCardsManager {
 public:
  ReadWriteCardsManager();
  virtual ~ReadWriteCardsManager();

  static ReadWriteCardsManager* Get();

  virtual void FetchController(
      const content::ContextMenuParams& params,
      content::BrowserContext* context,
      editor_menu::FetchControllerCallback callback) = 0;
};

}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_EDITOR_MENU_PUBLIC_CPP_READ_WRITE_CARDS_MANAGER_H_
