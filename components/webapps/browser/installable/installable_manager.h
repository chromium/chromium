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
#include "base/time/time.h"
#include "build/build_config.h"
#include "components/webapps/browser/installable/installable_data.h"
#include "components/webapps/browser/installable/installable_logging.h"
#include "components/webapps/browser/installable/installable_params.h"
#include "components/webapps/browser/installable/installable_task_queue.h"
#include "content/public/browser/installability_error.h"
#include "content/public/browser/service_worker_context.h"
#include "content/public/browser/service_worker_context_observer.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"
#include "third_party/blink/public/mojom/manifest/manifest.mojom.h"
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
  // Maximum dimension size in pixels for icons.
  static const int kMaximumIconSizeInPx =
#if BUILDFLAG(IS_ANDROID)
      std::numeric_limits<int>::max();
#else
      1024;
#endif

  explicit InstallableManager(content::WebContents* web_contents);

  InstallableManager(const InstallableManager&) = delete;
  InstallableManager& operator=(const InstallableManager&) = delete;

  ~InstallableManager() override;

  // Returns the minimum icon size in pixels for a site to be installable.
  static int GetMinimumIconSizeInPx();

  // Returns true if the overall security state of |web_contents| is sufficient
  // to be considered installable.
  static bool IsContentSecure(content::WebContents* web_contents);

  // Returns true for localhost and URLs that have been explicitly marked as
  // secure via a flag.
  static bool IsOriginConsideredSecure(const GURL& url);

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

 protected:
  // For mocking in tests.
  virtual void OnWaitingForServiceWorker() {}
  virtual void OnResetData() {}

 private:
  friend class content::WebContentsUserData<InstallableManager>;
  friend class AddToHomescreenDataFetcherTest;
  friend class InstallableManagerBrowserTest;
  friend class InstallableManagerOfflineCapabilityBrowserTest;
  friend class InstallableManagerUnitTest;
  friend class TestInstallableManager;
  FRIEND_TEST_ALL_PREFIXES(InstallableManagerBrowserTest,
                           ManagerBeginsInEmptyState);
  FRIEND_TEST_ALL_PREFIXES(InstallableManagerBrowserTest, ManagerInIncognito);
  FRIEND_TEST_ALL_PREFIXES(InstallableManagerBrowserTest,
                           CheckLazyServiceWorkerNoFetchHandlerFails);
  FRIEND_TEST_ALL_PREFIXES(InstallableManagerBrowserTest,
                           ManifestUrlChangeFlushesState);
  FRIEND_TEST_ALL_PREFIXES(InstallableManagerOfflineCapabilityBrowserTest,
                           CheckLazyServiceWorkerPassesWhenWaiting);
  FRIEND_TEST_ALL_PREFIXES(InstallableManagerOfflineCapabilityBrowserTest,
                           CheckWebapp);
  FRIEND_TEST_ALL_PREFIXES(InstallableManagerOfflineCapabilityBrowserTest,
                           CheckNotOfflineCapableStartUrl);
  FRIEND_TEST_ALL_PREFIXES(InstallableManagerInPrerenderingBrowserTest,
                           InstallableManagerInPrerendering);
  FRIEND_TEST_ALL_PREFIXES(InstallableManagerInPrerenderingBrowserTest,
                           NotifyManifestUrlChangedInActivation);
  FRIEND_TEST_ALL_PREFIXES(InstallableManagerInPrerenderingBrowserTest,
                           NotNotifyManifestUrlChangedInActivation);

  using IconPurpose = blink::mojom::ManifestImageResource_Purpose;

  enum class IconUsage { kPrimary, kSplash };

  struct EligiblityProperty {
    EligiblityProperty();
    ~EligiblityProperty();

    std::vector<InstallableStatusCode> errors;
    bool fetched = false;
  };

  struct ManifestProperty {
    ManifestProperty();
    ~ManifestProperty();

    InstallableStatusCode error = NO_ERROR_DETECTED;
    GURL url;
    blink::mojom::ManifestPtr manifest = blink::mojom::Manifest::New();
    bool fetched = false;
  };

  struct ValidManifestProperty {
    ValidManifestProperty();
    ~ValidManifestProperty();

    std::vector<InstallableStatusCode> errors;
    bool is_valid = false;
    bool fetched = false;
  };

  struct ServiceWorkerProperty {
    InstallableStatusCode error = NO_ERROR_DETECTED;
    bool has_worker = false;
    bool is_waiting = false;
    bool fetched = false;
  };

  struct IconProperty {
    IconProperty();

    // This class contains a std::unique_ptr and therefore must be move-only.
    IconProperty(const IconProperty&) = delete;
    IconProperty& operator=(const IconProperty&) = delete;

    IconProperty(IconProperty&& other) noexcept;
    IconProperty& operator=(IconProperty&& other);

    ~IconProperty();

    InstallableStatusCode error = NO_ERROR_DETECTED;
    IconPurpose purpose = blink::mojom::ManifestImageResource_Purpose::ANY;
    GURL url;
    std::unique_ptr<SkBitmap> icon;
    bool fetched = false;
  };

  // Returns true if an icon for the given usage is fetched successfully, or
  // doesn't need to fallback to another icon purpose (i.e. MASKABLE icon
  // allback to ANY icon).
  bool IsIconFetchComplete(IconUsage usage) const;

  // Returns true if we have tried fetching maskable icon. Note that this also
  // returns true if the fallback icon(IconPurpose::ANY) is fetched.
  bool IsMaskableIconFetched(IconUsage usage) const;

  // Sets the icon matching |usage| as fetched.
  void SetIconFetched(IconUsage usage);

  // Returns a vector with all errors encountered for the resources requested in
  // |params|, or an empty vector if there is no error.
  std::vector<InstallableStatusCode> GetErrors(const InstallableParams& params);

  // Gets/sets parts of particular properties. Exposed for testing.
  InstallableStatusCode eligibility_error() const;
  InstallableStatusCode manifest_error() const;
  InstallableStatusCode valid_manifest_error() const;
  void set_valid_manifest_error(InstallableStatusCode error_code);
  InstallableStatusCode worker_error() const;
  InstallableStatusCode icon_error(IconUsage usage);
  GURL& icon_url(IconUsage usage);
  const SkBitmap* icon(IconUsage usage);

  // Returns the WebContents to which this object is attached, or nullptr if the
  // WebContents doesn't exist or is currently being destroyed.
  content::WebContents* GetWebContents();

  // Returns true if |params| requires no more work to be done.
  bool IsComplete(const InstallableParams& params) const;

  // Resets members to empty and reports the given |error| to all queued tasks
  // to run queued callbacks before removing the tasks.
  // Called when navigating to a new page or if the WebContents is destroyed
  // whilst waiting for a callback.
  void Reset(InstallableStatusCode error);

  // Sets the fetched bit on the installable and icon subtasks.
  // Called if no manifest (or an empty manifest) was fetched from the site.
  void SetManifestDependentTasksComplete();

  // Methods coordinating and dispatching work for the current task.
  void CleanupAndStartNextTask();
  void RunCallback(InstallableTask task,
                   std::vector<InstallableStatusCode> errors);
  void WorkOnTask();

  // Data retrieval methods.
  void CheckEligiblity();
  void FetchManifest();
  void OnDidGetManifest(const GURL& manifest_url,
                        blink::mojom::ManifestPtr manifest);

  void CheckManifestValid(bool check_webapp_manifest_display);
  bool IsManifestValidForWebApp(const blink::mojom::Manifest& manifest,
                                bool check_webapp_manifest_display);
  void CheckServiceWorker();
  void OnDidCheckHasServiceWorker(
      base::TimeTicks check_service_worker_start_time,
      content::ServiceWorkerCapability capability);
  void OnDidCheckOfflineCapability(
      base::TimeTicks check_service_worker_start_time,
      bool enforce_offline_capability,
      content::OfflineCapability capability,
      int64_t service_worker_registration_id);

  void CheckAndFetchBestIcon(int ideal_icon_size_in_px,
                             int minimum_icon_size_in_px,
                             IconPurpose purpose,
                             IconUsage usage);
  void OnIconFetched(GURL icon_url, IconUsage usage, const SkBitmap& bitmap);

  void CheckAndFetchScreenshots();

  void OnScreenshotFetched(GURL screenshot_url, const SkBitmap& bitmap);
  void PopulateScreenshots(bool check_form_factor);

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
  bool valid_manifest();
  bool has_worker();

  InstallableTaskQueue task_queue_;

  // Installable properties cached on this object. When adding to this list,
  // make sure to update Reset().
  std::unique_ptr<EligiblityProperty> eligibility_;
  std::unique_ptr<ManifestProperty> manifest_;
  std::unique_ptr<ValidManifestProperty> valid_manifest_;
  std::unique_ptr<ServiceWorkerProperty> worker_;
  std::map<IconUsage, IconProperty> icons_;
  std::vector<Screenshot> screenshots_;

  // A map of screenshots downloaded. Used temporarily until images are moved to
  // the screenshots_ member.
  std::map<GURL, SkBitmap> downloaded_screenshots_;

  // The number of screenshots currently being downloaded.
  int screenshots_downloading_ = 0;

  // Whether all screenshots have been fetched.
  bool is_screenshots_fetch_complete_ = false;

  // Owned by the storage partition attached to the content::WebContents which
  // this object is scoped to.
  raw_ptr<content::ServiceWorkerContext> service_worker_context_;
  scoped_refptr<base::SequencedTaskRunner> sequenced_task_runner_;

  base::WeakPtrFactory<InstallableManager> weak_factory_{this};

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

}  // namespace webapps

#endif  // COMPONENTS_WEBAPPS_BROWSER_INSTALLABLE_INSTALLABLE_MANAGER_H_
