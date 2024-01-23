// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_MAHI_PUBLIC_CPP_MAHI_MANAGER_H_
#define CHROMEOS_COMPONENTS_MAHI_PUBLIC_CPP_MAHI_MANAGER_H_

#include <stdint.h>

#include <string>

#include "base/component_export.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"

namespace chromeos {

// An interface serves as the connection between mahi system and the UI.
class COMPONENT_EXPORT(MAHI_PUBLIC_CPP) MahiManager {
 public:
  MahiManager(const MahiManager&) = delete;
  MahiManager& operator=(const MahiManager&) = delete;

  static MahiManager* Get();

  // Opens the Mahi Panel in the display with `display_id`.
  virtual void OpenMahiPanel(int64_t display_id) = 0;

  // Returns the quick summary of the current active content on the
  // corresponding surface.
  using MahiSummaryCallback = base::OnceCallback<void(std::u16string)>;
  virtual void GetSummary(MahiSummaryCallback callback) = 0;

 protected:
  MahiManager();
  virtual ~MahiManager();
};

// A scoped object that set the global instance of
// `chromeos::MahiManager::Get()` to the provided object pointer. The real
// instance will be restored when this scoped object is destructed. This class
// is used in testing and mocking.
class COMPONENT_EXPORT(MAHI_PUBLIC_CPP) ScopedMahiManagerSetter {
 public:
  explicit ScopedMahiManagerSetter(MahiManager* manager);
  ScopedMahiManagerSetter(const ScopedMahiManagerSetter&) = delete;
  ScopedMahiManagerSetter& operator=(const ScopedMahiManagerSetter&) = delete;
  ~ScopedMahiManagerSetter();

 private:
  static ScopedMahiManagerSetter* instance_;

  raw_ptr<MahiManager> real_manager_instance_ = nullptr;
};

}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_MAHI_PUBLIC_CPP_MAHI_MANAGER_H_
