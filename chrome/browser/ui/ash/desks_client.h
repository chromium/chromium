// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_DESKS_CLIENT_H_
#define CHROME_BROWSER_UI_ASH_DESKS_CLIENT_H_

#include <memory>

namespace ash {
class DesksHelper;
class DeskTemplate;
}  // namespace ash

// Class to handle all Desks in-browser functionalities. Will call into
// ash::DesksController (via ash::DesksHelper) to do actual desk related
// operations.
class DesksClient {
 public:
  DesksClient();
  DesksClient(const DesksClient&) = delete;
  DesksClient& operator=(const DesksClient&) = delete;
  ~DesksClient();

  static DesksClient* Get();

  // Captures the active desk and returns it as a desk template containing
  // necessary information that can be used to recreate a same desk later.
  // Returns a nullptr if no such template can be captured. Note the captured
  // template will not be saved to storage.
  std::unique_ptr<ash::DeskTemplate> CaptureActiveDeskAsTemplate();

 private:
  ash::DesksHelper* const desks_helper_ = nullptr;
};

#endif  // CHROME_BROWSER_UI_ASH_DESKS_CLIENT_H_
