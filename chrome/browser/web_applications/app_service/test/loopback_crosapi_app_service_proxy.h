// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_APP_SERVICE_TEST_LOOPBACK_CROSAPI_APP_SERVICE_PROXY_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_APP_SERVICE_TEST_LOOPBACK_CROSAPI_APP_SERVICE_PROXY_H_

#include <stdint.h>
#include <string>
#include <vector>

#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/apps/app_service/app_service_proxy_lacros.h"
#include "chromeos/crosapi/mojom/app_service.mojom.h"
#include "chromeos/crosapi/mojom/app_service_types.mojom-forward.h"
#include "components/services/app_service/public/cpp/app_types.h"
#include "components/services/app_service/public/cpp/capability_access.h"
#include "components/services/app_service/public/cpp/icon_types.h"
#include "mojo/public/cpp/bindings/pending_remote.h"

class Profile;

static_assert(BUILDFLAG(IS_CHROMEOS_LACROS), "For Lacros only");

namespace web_app {

// Loops back AppService crosapi calls from lacros back to lacros for
// lacros-only testing of web apps.
//
// These calls would normally go from lacros to ash in production, but there is
// no ash to connect to in lacros-only tests, so most calls are dropped. This
// instead makes a best effort at emulating what the ash app service might do in
// response to calls so that listeners in lacros get notified of changes. Calls
// via this class are async to better simulate an interprocess communication.
class LoopbackCrosapiAppServiceProxy : public crosapi::mojom::AppServiceProxy,
                                       public crosapi::mojom::AppPublisher {
 public:
  explicit LoopbackCrosapiAppServiceProxy(Profile* profile);
  ~LoopbackCrosapiAppServiceProxy() override;

  // Removes app from handling supported links. This method is not needed by the
  // real crosapi AppServiceProxy interface so this directly updates the lacros
  // app service cache instead. This should be replaced by the crosapi
  // AppServiceProxy version if it's added there.
  void RemoveSupportedLinksPreference(const std::string& app_id);

 private:
  // crosapi::mojom::AppServiceProxy:
  void RegisterAppServiceSubscriber(
      mojo::PendingRemote<crosapi::mojom::AppServiceSubscriber> subscriber)
      override;
  void Launch(crosapi::mojom::LaunchParamsPtr launch_params) override;
  void LaunchWithResult(crosapi::mojom::LaunchParamsPtr launch_params,
                        LaunchWithResultCallback callback) override;
  void LoadIcon(const std::string& app_id,
                apps::IconKeyPtr icon_key,
                apps::IconType icon_type,
                int32_t size_hint_in_dip,
                apps::LoadIconCallback callback) override;
  void AddPreferredAppDeprecated(const std::string& app_id,
                                 crosapi::mojom::IntentPtr intent) override;
  void ShowAppManagementPage(const std::string& app_id) override;
  void SetSupportedLinksPreference(const std::string& app_id) override;
  void UninstallSilently(const std::string& app_id,
                         apps::UninstallSource uninstall_source) override;
  void InstallAppWithFallback(crosapi::mojom::InstallAppParamsPtr params,
                              InstallAppWithFallbackCallback callback) override;

  // crosapi::mojom::AppPublisher:
  void OnApps(std::vector<apps::AppPtr> deltas) override;
  void RegisterAppController(
      mojo::PendingRemote<crosapi::mojom::AppController> controller) override;
  void OnCapabilityAccesses(
      std::vector<apps::CapabilityAccessPtr> deltas) override;

  // Internal methods for enabling async calls.
  void PostTask(base::OnceClosure closure);
  void RemoveSupportedLinksPreferenceInternal(const std::string& app_id);
  void SetSupportedLinksPreferenceInternal(const std::string& app_id);
  void OnAppsInternal(std::vector<apps::AppPtr> deltas);

  base::WeakPtr<apps::AppServiceProxyLacros> app_service_;

  base::WeakPtrFactory<LoopbackCrosapiAppServiceProxy> weak_ptr_factory_{this};
};
}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_APP_SERVICE_TEST_LOOPBACK_CROSAPI_APP_SERVICE_PROXY_H_
