// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_DRIVE_PICKER_HOST_DRIVE_DISCLAIMER_CONTROLLER_H_
#define CHROME_BROWSER_UI_WEBUI_DRIVE_PICKER_HOST_DRIVE_DISCLAIMER_CONTROLLER_H_

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "components/contextual_search/footprints/public/constants.h"
#include "components/contextual_search/footprints/public/fpop_service.h"
#include "components/contextual_search/footprints/public/proto/footprints_oneplatform.pb.h"

namespace drive_picker {

class DriveDisclaimerController {
 public:
  enum class DisclaimerStatus {
    kAccepted,
    kNotAccepted,
    kRestricted,
  };

  explicit DriveDisclaimerController(
      std::unique_ptr<contextual_search::FpopService> fpop_service);
  ~DriveDisclaimerController();

  DriveDisclaimerController(const DriveDisclaimerController&) = delete;
  DriveDisclaimerController& operator=(const DriveDisclaimerController&) =
      delete;

  // Asynchronously verifies whether the user has accepted the required Drive
  // privacy disclaimer via the Footprints backend.
  void CheckDisclaimerStatusAsync(
      base::OnceCallback<void(DisclaimerStatus status)> completion_callback);

 private:
  void OnGetFacsResponse(
      base::OnceCallback<void(DisclaimerStatus status)> completion_callback,
      bool success,
      const footprints::oneplatform::GetFacsResponse& response);

  std::unique_ptr<contextual_search::FpopService> fpop_service_;
  base::WeakPtrFactory<DriveDisclaimerController> weak_factory_{this};
};

}  // namespace drive_picker

#endif  // CHROME_BROWSER_UI_WEBUI_DRIVE_PICKER_HOST_DRIVE_DISCLAIMER_CONTROLLER_H_
