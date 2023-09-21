// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_EDITOR_MENU_PUBLIC_CPP_READ_WRITE_CARDS_MANAGER_H_
#define CHROMEOS_COMPONENTS_EDITOR_MENU_PUBLIC_CPP_READ_WRITE_CARDS_MANAGER_H_

#include "base/component_export.h"

namespace content {
struct ContextMenuParams;
}  // namespace content

namespace chromeos {

class ReadWriteCardController;

// A manager to manage the controllers of Quick Answers or Editor Menu.
class COMPONENT_EXPORT(EDITOR_MENU_PUBLIC_CPP) ReadWriteCardsManager {
 public:
  ReadWriteCardsManager();
  virtual ~ReadWriteCardsManager();

  static ReadWriteCardsManager* Get();

  // Returns the supported controller for the input params. Could be nullptr if
  // it is not supported.
  virtual ReadWriteCardController* GetController(
      const content::ContextMenuParams& params) = 0;
};

}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_EDITOR_MENU_PUBLIC_CPP_READ_WRITE_CARDS_MANAGER_H_
