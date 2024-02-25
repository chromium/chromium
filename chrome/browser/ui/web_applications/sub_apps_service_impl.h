// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEB_APPLICATIONS_SUB_APPS_SERVICE_IMPL_H_
#define CHROME_BROWSER_UI_WEB_APPLICATIONS_SUB_APPS_SERVICE_IMPL_H_

#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "chrome/browser/web_applications/web_app_install_info.h"
#include "components/webapps/browser/install_result_code.h"
#include "components/webapps/common/web_app_id.h"
#include "content/public/browser/document_service.h"
#include "third_party/blink/public/mojom/subapps/sub_apps_service.mojom.h"

namespace content {
class RenderFrameHost;
}

namespace web_app {

namespace {

struct SubAppInstallParams {
  webapps::ManifestId manifest_id;
  GURL install_url;
};

struct SubAppInstallResult {
  webapps::ManifestId manifest_id;
  webapps::AppId app_id;
  webapps::InstallResultCode install_result_code;
};

}  // namespace

class SubAppsInstallDialogController;

class SubAppsServiceImpl
    : public content::DocumentService<blink::mojom::SubAppsService> {
 public:
  using AddResultsMojo = std::vector<blink::mojom::SubAppsServiceAddResultPtr>;

  static constexpr char kSubAppsUninstallNotificationId[] =
      "sub_apps_uninstall_notification";

  SubAppsServiceImpl(const SubAppsServiceImpl&) = delete;
  SubAppsServiceImpl& operator=(const SubAppsServiceImpl&) = delete;
  ~SubAppsServiceImpl() override;

  // We only want to create this object when the Browser* associated with the
  // WebContents is an Isolated Web App and when the RFH is the main frame.
  static void CreateIfAllowed(
      content::RenderFrameHost* render_frame_host,
      mojo::PendingReceiver<blink::mojom::SubAppsService> receiver);

  // blink::mojom::SubAppsService
  void Add(
      std::vector<blink::mojom::SubAppsServiceAddParametersPtr> sub_apps_to_add,
      AddCallback result_callback) override;
  void List(ListCallback result_callback) override;
  void Remove(const std::vector<std::string>& manifest_id_paths,
              RemoveCallback result_callback) override;

 private:
  struct AddCallInfo {
    AddCallInfo();
    ~AddCallInfo();

    AddCallback mojo_callback;
    std::vector<std::unique_ptr<WebAppInstallInfo>> install_infos;
    std::unique_ptr<SubAppsInstallDialogController> install_dialog;
    AddResultsMojo results;
  };

  void CollectInstallData(int add_call_id,
                          std::vector<SubAppInstallParams> requested_installs,
                          webapps::ManifestId parent_manifest_id);
  void ProcessInstallData(
      int add_call_id,
      std::vector<std::pair<webapps::ManifestId,
                            std::unique_ptr<WebAppInstallInfo>>> install_data);
  void ScheduleSubAppInstalls(int add_call_id);
  void ProcessDialogResponse(int add_call_id, bool dialog_accepted);
  void FinishAddCallOrShowInstallDialog(int add_call_id);
  void FinishAddCall(int add_call_id,
                     std::vector<SubAppInstallResult> install_results);

  void RemoveSubApp(
      const std::string& manifest_id_path,
      base::OnceCallback<void(blink::mojom::SubAppsServiceRemoveResultPtr)>
          remove_barrier_callback,
      const webapps::AppId* calling_app_id);
  void NotifyUninstall(
      RemoveCallback result_callback,
      std::vector<blink::mojom::SubAppsServiceRemoveResultPtr> remove_results);

  SubAppsServiceImpl(
      content::RenderFrameHost& render_frame_host,
      mojo::PendingReceiver<blink::mojom::SubAppsService> receiver);

  int next_add_call_id_ = 0;
  std::map<int, AddCallInfo> add_call_info_;

  base::WeakPtrFactory<SubAppsServiceImpl> weak_ptr_factory_{this};
};

}  // namespace web_app

#endif  // CHROME_BROWSER_UI_WEB_APPLICATIONS_SUB_APPS_SERVICE_IMPL_H_
