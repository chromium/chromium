// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_BOCA_SPOTLIGHT_SPOTLIGHT_SERVICE_H_
#define CHROMEOS_ASH_COMPONENTS_BOCA_SPOTLIGHT_SPOTLIGHT_SERVICE_H_
#include <memory>
#include <optional>

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "chromeos/ash/components/boca/proto/session.pb.h"
#include "chromeos/ash/components/boca/spotlight/register_screen_request.h"
#include "chromeos/ash/components/boca/spotlight/update_view_screen_state_request.h"
#include "chromeos/ash/components/boca/spotlight/view_screen_request.h"

namespace google_apis {
class RequestSender;
}

namespace ash::boca {

class BocaSessionManager;

// A separate request sender for spotlight action. It can be in parallel with
// other session requests.
class SpotlightService {
 public:
  // `boca_session_manager` must not be nullptr and must outlive this.
  explicit SpotlightService(BocaSessionManager* boca_session_manager);
  SpotlightService(BocaSessionManager* boca_session_manager,
                   std::unique_ptr<google_apis::RequestSender> sender);
  SpotlightService(const SpotlightService&) = delete;
  SpotlightService& operator=(const SpotlightService&) = delete;
  virtual ~SpotlightService();

  virtual void ViewScreen(std::string student_gaia_id,
                          std::string url_base,
                          ViewScreenRequestCallback callback);

  virtual void RegisterScreen(const std::string& connection_code,
                              std::string url_base,
                              RegisterScreenRequestCallback callback);

  virtual void UpdateViewScreenState(
      std::string student_gaia_id,
      ::boca::ViewScreenConfig::ViewScreenState view_screen_state,
      std::string url_base,
      UpdateViewScreenStateRequestCallback callback);

  google_apis::RequestSender* sender() { return sender_.get(); }

 private:
  std::optional<std::string> ValidateStudentAndGetDeviceId(
      ::boca::Session* current_session,
      const std::string& student_gaia_id);

  const raw_ref<BocaSessionManager> boca_session_manager_;
  std::unique_ptr<google_apis::RequestSender> sender_;
};
}  // namespace ash::boca

#endif  // CHROMEOS_ASH_COMPONENTS_BOCA_SPOTLIGHT_SPOTLIGHT_SERVICE_H_
