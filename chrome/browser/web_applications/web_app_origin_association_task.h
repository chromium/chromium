// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_ORIGIN_ASSOCIATION_TASK_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_ORIGIN_ASSOCIATION_TASK_H_

#include <string>
#include <vector>

#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/web_applications/web_app_origin_association_manager.h"

class GURL;
namespace web_app {

// A task that contains the information needed to get associations. It fetches
// and parses web app origin associations, sets the scope for valid scope
// extensions, and sends them back. Invalid scope extensions will not be added
// to result and not sent back to the caller.
class WebAppOriginAssociationManager::Task {
 public:
  Task(const GURL& web_app_identity,
       ScopeExtensions scope_extensions,
       WebAppOriginAssociationManager& manager,
       OnDidGetWebAppOriginAssociations callback);
  Task(Task&& other) = delete;
  Task& operator=(const Task&) = delete;
  ~Task();

  void Start();

 private:
  ScopeExtensionInfo& GetCurrentScopeExtension();
  void FetchAssociationFile(ScopeExtensionInfo& scope_extension);
  void OnAssociationFileFetched(std::unique_ptr<std::string> file_content);
  void OnAssociationParsed(
      webapps::mojom::WebAppOriginAssociationPtr association,
      std::vector<webapps::mojom::WebAppOriginAssociationErrorPtr> errors);
  void MaybeStartNextScopeExtension();
  // Schedule a task to run |callback_| on the UI thread and notify |manager_|
  // that this task is completed.
  void Finalize();

  const GURL web_app_identity_;
  // Pending scope extensions that need to be processed to get associations.
  std::deque<ScopeExtensionInfo> pending_scope_extensions_;
  // The manager that owns this task.
  const raw_ref<WebAppOriginAssociationManager> owner_;
  // Callback to send the result back.
  OnDidGetWebAppOriginAssociations callback_;
  ScopeExtensions result_;

  base::WeakPtrFactory<Task> weak_ptr_factory_{this};
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_ORIGIN_ASSOCIATION_TASK_H_
