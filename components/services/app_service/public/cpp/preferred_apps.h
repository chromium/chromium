// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SERVICES_APP_SERVICE_PUBLIC_CPP_PREFERRED_APPS_H_
#define COMPONENTS_SERVICES_APP_SERVICE_PUBLIC_CPP_PREFERRED_APPS_H_

#include <map>

#include "base/callback.h"
#include "base/containers/queue.h"
#include "base/files/file_path.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "components/services/app_service/public/cpp/preferred_apps_list.h"
#include "components/services/app_service/public/mojom/app_service.mojom.h"
#include "components/services/app_service/public/mojom/types.mojom.h"

namespace apps {

class AppServiceMojomImpl;

// The implementation of the preferred apps to manage the PreferredAppsList.
class PreferredApps {
 public:
  class Host {
   public:
    Host() = default;
    Host(const Host&) = delete;
    Host& operator=(const Host&) = delete;
    ~Host() = default;

    virtual void InitializePreferredAppsForAllSubscribers() = 0;

    virtual void OnPreferredAppsChanged(
        apps::mojom::PreferredAppChangesPtr changes) = 0;

    virtual void OnPreferredAppSet(
        const std::string& app_id,
        apps::mojom::IntentFilterPtr intent_filter,
        apps::mojom::IntentPtr intent,
        apps::mojom::ReplacedAppPreferencesPtr replaced_app_preferences) = 0;

    virtual void OnSupportedLinksPreferenceChanged(const std::string& app_id,
                                                   bool open_in_app) = 0;

    // Returns publisher for `app_type`, or nullptr if there is no publisher for
    // `app_type`.
    virtual apps::mojom::Publisher* GetPublisher(
        apps::mojom::AppType app_type) = 0;
  };

  PreferredApps(
      Host* host,
      const base::FilePath& profile_dir,
      base::OnceClosure read_completed_for_testing = base::OnceClosure(),
      base::OnceClosure write_completed_for_testing = base::OnceClosure());

  PreferredApps(const PreferredApps&) = delete;
  PreferredApps& operator=(const PreferredApps&) = delete;

  ~PreferredApps();

  void AddPreferredApp(apps::mojom::AppType app_type,
                       const std::string& app_id,
                       apps::mojom::IntentFilterPtr intent_filter,
                       apps::mojom::IntentPtr intent,
                       bool from_publisher);
  void RemovePreferredApp(apps::mojom::AppType app_type,
                          const std::string& app_id);
  void RemovePreferredAppForFilter(apps::mojom::AppType app_type,
                                   const std::string& app_id,
                                   apps::mojom::IntentFilterPtr intent_filter);
  void SetSupportedLinksPreference(
      apps::mojom::AppType app_type,
      const std::string& app_id,
      std::vector<apps::mojom::IntentFilterPtr> all_link_filters);
  void RemoveSupportedLinksPreference(apps::mojom::AppType app_type,
                                      const std::string& app_id);

 private:
  friend AppServiceMojomImpl;

  // Initialize the preferred apps from disk.
  void InitializePreferredApps();

  // Write the preferred apps to a json file.
  void WriteToJSON(const base::FilePath& profile_dir,
                   const apps::PreferredAppsList& preferred_apps);

  void WriteCompleted();

  void ReadFromJSON(const base::FilePath& profile_dir);

  void ReadCompleted(std::string preferred_apps_string);

  // Runs |task| after the PreferredAppsList is fully initialized. |task| will
  // be run immediately if preferred apps are already initialized.
  void RunAfterPreferredAppsReady(base::OnceClosure task);

  void AddPreferredAppImpl(apps::mojom::AppType app_type,
                           const std::string& app_id,
                           apps::mojom::IntentFilterPtr intent_filter,
                           apps::mojom::IntentPtr intent,
                           bool from_publisher);
  void RemovePreferredAppImpl(apps::mojom::AppType app_type,
                              const std::string& app_id);
  void RemovePreferredAppForFilterImpl(
      apps::mojom::AppType app_type,
      const std::string& app_id,
      apps::mojom::IntentFilterPtr intent_filter);
  void SetSupportedLinksPreferenceImpl(
      apps::mojom::AppType app_type,
      const std::string& app_id,
      std::vector<apps::mojom::IntentFilterPtr> all_link_filters);
  void RemoveSupportedLinksPreferenceImpl(apps::mojom::AppType app_type,
                                          const std::string& app_id);

  // `host_` owns `this`.
  raw_ptr<Host> host_;

  PreferredAppsList preferred_apps_list_;

  base::FilePath profile_dir_;

  // True if need to write preferred apps to file after the current write is
  // completed.
  bool should_write_preferred_apps_to_file_ = false;

  // True if it is currently writing preferred apps to file.
  bool writing_preferred_apps_ = false;

  // Task runner where the file operations takes place. This is to make sure the
  // write operation will be operated in sequence.
  scoped_refptr<base::SequencedTaskRunner> const task_runner_;

  base::OnceClosure read_completed_for_testing_;

  base::OnceClosure write_completed_for_testing_;

  base::queue<base::OnceClosure> pending_preferred_apps_tasks_;

  base::WeakPtrFactory<PreferredApps> weak_ptr_factory_{this};
};

}  // namespace apps

#endif  // COMPONENTS_SERVICES_APP_SERVICE_PUBLIC_CPP_PREFERRED_APPS_H_
