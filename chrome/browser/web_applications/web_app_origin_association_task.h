// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_ORIGIN_ASSOCIATION_TASK_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_ORIGIN_ASSOCIATION_TASK_H_

#include <deque>
#include <map>
#include <optional>
#include <string>
#include <vector>

#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/web_applications/web_app_origin_association_manager.h"
#include "components/webapps/services/web_app_origin_association/web_app_origin_association_parser.h"

class GURL;
namespace web_app {

// A task that contains the information needed to get associations. It fetches
// and parses web app origin associations, sets the scope for valid scope
// extensions, and sends them back. Invalid scope extensions will not be added
// to result and not sent back to the caller.
class WebAppOriginAssociationManager::Task {
 public:
  Task(const GURL& web_app_identity,
       OriginAssociations origin_associations,
       WebAppOriginAssociationManager& manager,
       OnDidGetWebAppOriginAssociations callback);
  Task(Task&& other) = delete;
  Task& operator=(const Task&) = delete;
  ~Task();

  void Start();

 private:
  void FetchAssociationFile(const url::Origin& origin);
  void OnAssociationFileFetched(const url::Origin& origin,
                                std::optional<std::string> file_content);
  void MaybeStartNextStep();
  // Schedule a task to run |callback_| on the UI thread and notify |manager_|
  // that this task is completed.
  void Finalize();

  const GURL web_app_identity_;
  std::deque<url::Origin> pending_origins_;
  std::map<url::Origin, webapps::AssociatedWebApp> fetched_associations_;
  ScopeExtensions scope_extensions_input_;
  std::vector<web_app::proto::WebAppMigrationSource> migration_sources_input_;
  // The manager that owns this task.
  const raw_ref<WebAppOriginAssociationManager> owner_;
  // Callback to send the result back.
  OnDidGetWebAppOriginAssociations callback_;
  OriginAssociations result_;

  base::WeakPtrFactory<Task> weak_ptr_factory_{this};
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_ORIGIN_ASSOCIATION_TASK_H_
