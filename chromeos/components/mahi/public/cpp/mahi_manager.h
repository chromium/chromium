// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_MAHI_PUBLIC_CPP_MAHI_MANAGER_H_
#define CHROMEOS_COMPONENTS_MAHI_PUBLIC_CPP_MAHI_MANAGER_H_

#include <stdint.h>

#include "base/component_export.h"

namespace chromeos {

// An interface serves as the connection between mahi system and the UI.
class COMPONENT_EXPORT(MAHI_PUBLIC_CPP) MahiManager {
 public:
  MahiManager(const MahiManager&) = delete;
  MahiManager& operator=(const MahiManager&) = delete;

  static MahiManager* Get();

  // Opens the Mahi Panel in the display with `display_id`.
  virtual void OpenMahiPanel(int64_t display_id) = 0;

 protected:
  MahiManager();
  virtual ~MahiManager();
};

}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_MAHI_PUBLIC_CPP_MAHI_MANAGER_H_
