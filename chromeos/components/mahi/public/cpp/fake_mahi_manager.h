// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_MAHI_PUBLIC_CPP_FAKE_MAHI_MANAGER_H_
#define CHROMEOS_COMPONENTS_MAHI_PUBLIC_CPP_FAKE_MAHI_MANAGER_H_

#include <string>

#include "base/component_export.h"
#include "chromeos/components/mahi/public/cpp/mahi_manager.h"

namespace chromeos {

// A fake implementation of `MahiManager`.
class COMPONENT_EXPORT(MAHI_PUBLIC_CPP) FakeMahiManager : public MahiManager {
 public:
  FakeMahiManager() = default;
  FakeMahiManager(const FakeMahiManager&) = delete;
  FakeMahiManager& operator=(const FakeMahiManager&) = delete;
  ~FakeMahiManager() override = default;

  // MahiManager:
  void GetSummary(MahiSummaryCallback callback) override;
  void OpenMahiPanel(int64_t display_id) override {}

  void set_summary_text(std::u16string summary_text) {
    summary_text_ = summary_text;
  }

 private:
  std::u16string summary_text_;
};

}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_MAHI_PUBLIC_CPP_FAKE_MAHI_MANAGER_H_
