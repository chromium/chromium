// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SERVICES_APP_SERVICE_PUBLIC_CPP_PREFERRED_APPS_IMPL_H_
#define COMPONENTS_SERVICES_APP_SERVICE_PUBLIC_CPP_PREFERRED_APPS_IMPL_H_

#include <map>

#include "base/containers/queue.h"
#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "components/services/app_service/public/cpp/app_types.h"
#include "components/services/app_service/public/cpp/intent.h"
#include "components/services/app_service/public/cpp/intent_filter.h"
#include "components/services/app_service/public/cpp/preferred_app.h"
#include "components/services/app_service/public/cpp/preferred_apps_list.h"
#include "components/services/app_service/public/cpp/preferred_apps_list_handle.h"

namespace apps {

class AppServiceMojomImpl;
class AppServiceProxyPreferredAppsTest;

// The implementation of the preferred apps to manage the PreferredAppsList.
class PreferredAppsImpl {
 public:
  class Host {
   public:
    Host() = default;
    Host(const Host&) = delete;
    Host& operator=(const Host&) = delete;
    ~Host() = default;

    // Called when the PreferredAppsList has been loaded from disk, and can
    // be used to initialize subscribers.
    // Only implemented in Ash, to support initializing the Lacros copy of the
    // PreferredAppsList.
    virtual void InitializePreferredAppsForAllSubscribers() {}

    // Called when changes have been made to the PreferredAppsList which should
    // be propagated to subscribers.
    // Only implemented in Ash, to support updating the Lacros copy of the
    // PreferredAppsList.
    virtual void OnPreferredAppsChanged(PreferredAppChangesPtr changes) {}

    // Notifies the host that the supported links preference for a particular
    // `app_id` was enabled/disabled. Used by the host to notify the app
    // publisher (if any) of the change.
    virtual void OnSupportedLinksPreferenceChanged(const std::string& app_id,
                                                   bool open_in_app) = 0;
  };

  PreferredAppsImpl(
      Host* host,
      const base::FilePath& profile_dir,
      base::OnceClosure read_completed_for_testing = base::OnceClosure(),
      base::OnceClosure write_completed_for_testing = base::OnceClosure());

  PreferredAppsImpl(const PreferredAppsImpl&) = delete;
  PreferredAppsImpl& operator=(const PreferredAppsImpl&) = delete;

  ~PreferredAppsImpl();

  void RemovePreferredApp(const std::string& app_id);
  void SetSupportedLinksPreference(const std::string& app_id,
                                   IntentFilters all_link_filters);
  void RemoveSupportedLinksPreference(const std::string& app_id);

  PreferredAppsListHandle& preferred_apps_list() {
    return preferred_apps_list_;
  }

 private:
  friend AppServiceMojomImpl;
  friend class AppServiceProxyPreferredAppsTest;

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

  void RemovePreferredAppImpl(const std::string& app_id);
  void SetSupportedLinksPreferenceImpl(const std::string& app_id,
                                       IntentFilters all_link_filters);
  void RemoveSupportedLinksPreferenceImpl(const std::string& app_id);

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

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<PreferredAppsImpl> weak_ptr_factory_{this};
};

}  // namespace apps

#endif  // COMPONENTS_SERVICES_APP_SERVICE_PUBLIC_CPP_PREFERRED_APPS_IMPL_H_
