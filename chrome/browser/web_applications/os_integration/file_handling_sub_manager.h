// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_OS_INTEGRATION_FILE_HANDLING_SUB_MANAGER_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_OS_INTEGRATION_FILE_HANDLING_SUB_MANAGER_H_

#include "base/files/file_path.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/web_applications/os_integration/os_integration_sub_manager.h"
#include "chrome/browser/web_applications/proto/web_app_os_integration_state.pb.h"
#include "components/webapps/common/web_app_id.h"

namespace web_app {

class WebAppProvider;

std::set<std::string> GetFileExtensionsFromFileHandlingProto(
    const proto::FileHandling& file_handling);

std::set<std::string> GetMimeTypesFromFileHandlingProto(
    const proto::FileHandling& file_handling);

// Used to track updates to the file handlers for a web app.
class FileHandlingSubManager : public OsIntegrationSubManager {
 public:
  FileHandlingSubManager(const base::FilePath& profile_path,
                         WebAppProvider& provider);
  ~FileHandlingSubManager() override;

  void Configure(const webapps::AppId& app_id,
                 proto::WebAppOsIntegrationState& desired_state,
                 base::OnceClosure configure_done) override;
  void Execute(const webapps::AppId& app_id,
               const std::optional<SynchronizeOsOptions>& synchronize_options,
               const proto::WebAppOsIntegrationState& desired_state,
               const proto::WebAppOsIntegrationState& current_state,
               base::OnceClosure callback) override;
  void ForceUnregister(const webapps::AppId& app_id,
                       base::OnceClosure callback) override;

 private:
  void Unregister(const webapps::AppId& app_id,
                  const proto::WebAppOsIntegrationState& desired_state,
                  const proto::WebAppOsIntegrationState& current_state,
                  base::OnceClosure callback);

  void Register(const webapps::AppId& app_id,
                const proto::WebAppOsIntegrationState& desired_state,
                base::OnceClosure callback);

  const base::FilePath profile_path_;
  const raw_ref<WebAppProvider> provider_;

  base::WeakPtrFactory<FileHandlingSubManager> weak_ptr_factory_{this};
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_OS_INTEGRATION_FILE_HANDLING_SUB_MANAGER_H_
