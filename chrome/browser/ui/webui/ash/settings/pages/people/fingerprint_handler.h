// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_SETTINGS_PAGES_PEOPLE_FINGERPRINT_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_SETTINGS_PAGES_PEOPLE_FINGERPRINT_HANDLER_H_

#include "base/containers/flat_map.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "components/session_manager/core/session_manager.h"
#include "components/session_manager/core/session_manager_observer.h"
#include "content/public/browser/web_ui_message_handler.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/device/public/mojom/fingerprint.mojom.h"

class Profile;

namespace ash::settings {

// Chrome OS fingerprint setup settings page UI handler.
class FingerprintHandler : public content::WebUIMessageHandler,
                           public device::mojom::FingerprintObserver,
                           public session_manager::SessionManagerObserver {
 public:
  explicit FingerprintHandler(Profile* profile);

  FingerprintHandler(const FingerprintHandler&) = delete;
  FingerprintHandler& operator=(const FingerprintHandler&) = delete;

  ~FingerprintHandler() override;

  // content::WebUIMessageHandler overrides:
  void RegisterMessages() override;
  void OnJavascriptAllowed() override;
  void OnJavascriptDisallowed() override;

  // device::mojom::FingerprintObserver:
  void OnRestarted() override;
  void OnStatusChanged(device::mojom::BiometricsManagerStatus status) override;
  void OnEnrollScanDone(device::mojom::ScanResult scan_result,
                        bool enroll_session_complete,
                        int percent_complete) override;
  void OnAuthScanDone(
      const device::mojom::FingerprintMessagePtr msg,
      const base::flat_map<std::string, std::vector<std::string>>& matches)
      override;
  void OnSessionFailed() override;

  // session_manager::SessionManagerObserver:
  void OnSessionStateChanged() override;

 private:
  void HandleGetFingerprintsList(const base::ListValue& args);
  void HandleGetNumFingerprints(const base::ListValue& args);
  void HandleStartEnroll(const base::ListValue& args);
  void HandleCancelCurrentEnroll(const base::ListValue& args);
  void HandleGetEnrollmentLabel(const base::ListValue& args);
  void HandleRemoveEnrollment(const base::ListValue& args);
  void HandleChangeEnrollmentLabel(const base::ListValue& args);

  void OnGetFingerprintsList(
      const std::string& callback_id,
      const base::flat_map<std::string, std::string>& fingerprints_list_mapping,
      bool success);
  void OnRequestRecordLabel(const std::string& callback_id,
                            const std::string& label);
  void OnCancelCurrentEnrollSession(bool success);
  void OnRemoveRecord(const std::string& callback_id, bool success);
  void OnSetRecordLabel(const std::string& callback_id, bool success);
  void OnEndCurrentAuthSession(bool success);
  bool CheckAuthTokenValidity(const std::string& auth_token);

  raw_ptr<Profile> profile_;  // unowned

  std::vector<std::string> fingerprints_labels_;
  std::vector<std::string> fingerprints_paths_;
  std::string user_id_;

  mojo::Remote<device::mojom::Fingerprint> fp_service_;
  mojo::Receiver<device::mojom::FingerprintObserver> receiver_{this};
  base::ScopedObservation<session_manager::SessionManager,
                          session_manager::SessionManagerObserver>
      session_observation_{this};

  base::WeakPtrFactory<FingerprintHandler> weak_ptr_factory_{this};
};

}  // namespace ash::settings

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_SETTINGS_PAGES_PEOPLE_FINGERPRINT_HANDLER_H_
