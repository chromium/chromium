// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEBAPPS_BROWSER_INSTALLABLE_INSTALLABLE_MANAGER_H_
#define COMPONENTS_WEBAPPS_BROWSER_INSTALLABLE_INSTALLABLE_MANAGER_H_

#include <limits>
#include <map>
#include <memory>
#include <vector>

#include "base/functional/callback_forward.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "components/webapps/browser/installable/installable_data.h"
#include "components/webapps/browser/installable/installable_logging.h"
#include "components/webapps/browser/installable/installable_page_data.h"
#include "components/webapps/browser/installable/installable_params.h"
#include "components/webapps/browser/installable/installable_task_queue.h"
#include "content/public/browser/installability_error.h"
#include "content/public/browser/service_worker_context.h"
#include "content/public/browser/service_worker_context_observer.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "url/gurl.h"

namespace webapps {

// This class is responsible for fetching the resources required to check and
// install a site.
class InstallableManager
    : public content::ServiceWorkerContextObserver,
      public content::WebContentsObserver,
      public content::WebContentsUserData<InstallableManager> {
 public:
  explicit InstallableManager(content::WebContents* web_contents);

  InstallableManager(const InstallableManager&) = delete;
  InstallableManager& operator=(const InstallableManager&) = delete;

  ~InstallableManager() override;

  // Get the installable data, fetching the resources specified in |params|.
  // |callback| is invoked synchronously (i.e. not via PostTask on the UI thread
  // when the data is ready; the synchronous execution ensures that the
  // references |callback| receives in its InstallableData argument are valid.
  //
  // |callback| may never be invoked if |params.wait_for_worker| is true, or if
  // the user navigates the page before fetching is complete.
  //
  // Calls requesting data that has already been fetched will return the cached
  // data.
  virtual void GetData(const InstallableParams& params,
                       InstallableCallback callback);

  // Runs the full installability check, and when finished, runs |callback|
  // passing a list of human-readable strings describing the errors encountered
  // during the run. The list is empty if no errors were encountered.
  void GetAllErrors(
      base::OnceCallback<void(std::vector<content::InstallabilityError>
                                  installability_errors)> callback);

  void GetPrimaryIcon(
      base::OnceCallback<void(const SkBitmap* primaryIcon)> callback);

  void SetSequencedTaskRunnerForTesting(
      scoped_refptr<base::SequencedTaskRunner> task_runner);

  void OnTaskFinished();
  virtual void OnTaskPaused();

 protected:
  // For mocking in tests.
  virtual void OnResetData() {}

 private:
  friend class content::WebContentsUserData<InstallableManager>;
  friend class InstallableManagerBrowserTest;
  friend class TestInstallableManager;

  FRIEND_TEST_ALL_PREFIXES(InstallableManagerBrowserTest,
                           ManagerBeginsInEmptyState);
  FRIEND_TEST_ALL_PREFIXES(InstallableManagerBrowserTest, ManagerInIncognito);
  FRIEND_TEST_ALL_PREFIXES(InstallableManagerBrowserTest,
                           CheckLazyServiceWorkerNoFetchHandlerFails);
  FRIEND_TEST_ALL_PREFIXES(InstallableManagerBrowserTest,
                           ManifestUrlChangeFlushesState);
  FRIEND_TEST_ALL_PREFIXES(InstallableManagerBrowserTest,
                           CheckLazyServiceWorkerPassesWhenWaiting);
  FRIEND_TEST_ALL_PREFIXES(InstallableManagerBrowserTest, CheckWebapp);
  FRIEND_TEST_ALL_PREFIXES(InstallableManagerInPrerenderingBrowserTest,
                           InstallableManagerInPrerendering);
  FRIEND_TEST_ALL_PREFIXES(InstallableManagerInPrerenderingBrowserTest,
                           NotifyManifestUrlChangedInActivation);
  FRIEND_TEST_ALL_PREFIXES(InstallableManagerInPrerenderingBrowserTest,
                           NotNotifyManifestUrlChangedInActivation);

  using IconPurpose = blink::mojom::ManifestImageResource_Purpose;

  // Gets/sets parts of particular properties. Exposed for testing.
  InstallableStatusCode manifest_error() const;
  InstallableStatusCode worker_error() const;
  InstallableStatusCode icon_error() const;
  GURL icon_url() const;
  const SkBitmap* icon() const;

  // Returns the WebContents to which this object is attached, or nullptr if the
  // WebContents doesn't exist or is currently being destroyed.
  content::WebContents* GetWebContents();

  // Resets members to empty and reports the given |error| to all queued tasks
  // to run queued callbacks before removing the tasks.
  // Called when navigating to a new page or if the WebContents is destroyed
  // whilst waiting for a callback.
  void Reset(InstallableStatusCode error);

  // Methods coordinating and dispatching work for the current task.
  void FinishAndStartNextTask();

  // content::ServiceWorkerContextObserver overrides
  void OnRegistrationCompleted(const GURL& pattern) override;
  void OnDestruct(content::ServiceWorkerContext* context) override;

  // content::WebContentsObserver overrides
  void PrimaryPageChanged(content::Page& page) override;
  void DidUpdateWebManifestURL(content::RenderFrameHost* rfh,
                               const GURL& manifest_url) override;
  void WebContentsDestroyed() override;

  const GURL& manifest_url() const;
  const blink::mojom::Manifest& manifest() const;
  bool has_worker() const;

  std::unique_ptr<InstallablePageData> page_data_;
  InstallableTaskQueue task_queue_;

  // Owned by the storage partition attached to the content::WebContents which
  // this object is scoped to.
  raw_ptr<content::ServiceWorkerContext> service_worker_context_;
  scoped_refptr<base::SequencedTaskRunner> sequenced_task_runner_;

  base::WeakPtrFactory<InstallableManager> weak_factory_{this};

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

}  // namespace webapps

#endif  // COMPONENTS_WEBAPPS_BROWSER_INSTALLABLE_INSTALLABLE_MANAGER_H_
