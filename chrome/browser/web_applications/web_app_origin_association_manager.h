// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_ORIGIN_ASSOCIATION_MANAGER_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_ORIGIN_ASSOCIATION_MANAGER_H_

#include <deque>
#include <memory>
#include <vector>

#include "base/gtest_prod_util.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/web_applications/proto/web_app.pb.h"
#include "chrome/browser/web_applications/scope_extension_info.h"
#include "components/webapps/services/web_app_origin_association/web_app_origin_association_fetcher.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/public/common/manifest/manifest.h"
#include "url/origin.h"

namespace web_app {

// Result of fetching, parsing, and validating web app origin association files.
struct OriginAssociations {
  OriginAssociations();
  OriginAssociations(const OriginAssociations&);
  OriginAssociations(OriginAssociations&&);
  OriginAssociations& operator=(const OriginAssociations&);
  OriginAssociations& operator=(OriginAssociations&&);
  ~OriginAssociations();

  bool operator==(const OriginAssociations&) const;

  ScopeExtensions scope_extensions;
  std::vector<web_app::proto::WebAppMigrationSource> migration_sources;
};

// Callback type that sends back the valid |origin_associations|.
using OnDidGetWebAppOriginAssociations =
    base::OnceCallback<void(OriginAssociations origin_associations)>;

// Fetch, parse, and validate web app origin association files.
class WebAppOriginAssociationManager {
 public:
  // Does the fetching, parsing, and validation work for a batch of scope
  // extensions.
  class Task;

  WebAppOriginAssociationManager();
  WebAppOriginAssociationManager(const WebAppOriginAssociationManager&) =
      delete;
  WebAppOriginAssociationManager& operator=(
      const WebAppOriginAssociationManager&) = delete;
  virtual ~WebAppOriginAssociationManager();

  virtual void GetWebAppOriginAssociations(
      const GURL& web_app_identity,
      OriginAssociations origin_associations,
      OnDidGetWebAppOriginAssociations callback);

  void SetFetcherForTest(
      std::unique_ptr<webapps::WebAppOriginAssociationFetcher> fetcher);
  webapps::WebAppOriginAssociationFetcher& GetFetcherForTest();

 private:
  FRIEND_TEST_ALL_PREFIXES(WebAppOriginAssociationManagerTest, RunTasks);

  webapps::WebAppOriginAssociationFetcher& GetFetcher();
  void MaybeStartNextTask();
  void OnTaskCompleted();

  std::deque<std::unique_ptr<Task>> pending_tasks_;
  bool task_in_progress_ = false;

  std::unique_ptr<webapps::WebAppOriginAssociationFetcher> fetcher_;
  base::WeakPtrFactory<WebAppOriginAssociationManager> weak_ptr_factory_{this};
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_ORIGIN_ASSOCIATION_MANAGER_H_
