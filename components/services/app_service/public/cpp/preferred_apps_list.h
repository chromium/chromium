// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SERVICES_APP_SERVICE_PUBLIC_CPP_PREFERRED_APPS_LIST_H_
#define COMPONENTS_SERVICES_APP_SERVICE_PUBLIC_CPP_PREFERRED_APPS_LIST_H_

#include <memory>
#include <string>
#include <vector>

#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "components/services/app_service/public/mojom/types.mojom.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

class GURL;

namespace apps {

// The preferred apps set by the user. The preferred apps is stored as
// an list of |intent_filter| vs. app_id.
class PreferredAppsList {
 public:
  class Observer : public base::CheckedObserver {
   public:
    virtual void OnPreferredAppChanged(const std::string& app_id,
                                       bool is_preferred_app) = 0;

    // Called when the PreferredAppsList object (the thing that this observer
    // observes) will be destroyed. In response, the observer, |this|, should
    // call "cache->RemoveObserver(this)", whether directly or indirectly (e.g.
    // via base::ScopedObservation::Remove or via Observe(nullptr)).
    virtual void OnPreferredAppsListWillBeDestroyed(
        PreferredAppsList* list) = 0;

    Observer(const Observer&) = delete;
    Observer& operator=(const Observer&) = delete;

   protected:
    // Use this constructor when the observer |this| is tied to a single
    // PreferredAppsList for its entire lifetime, or until the observee (the
    // PreferredAppsList) is destroyed, whichever comes first.
    explicit Observer(PreferredAppsList* list);

    // Use this constructor when the observer |this| wants to observe a
    // PreferredAppsList for part of its lifetime. It can then call Observe() to
    // start and stop observing.
    Observer();
    ~Observer() override;

    // Start observing a different PreferredAppsList. |cache| may be nullptr,
    // meaning to stop observing.
    void Observe(PreferredAppsList* list);

   private:
    PreferredAppsList* list_ = nullptr;
  };

  PreferredAppsList();
  ~PreferredAppsList();

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  PreferredAppsList(const PreferredAppsList&) = delete;
  PreferredAppsList& operator=(const PreferredAppsList&) = delete;

  using PreferredApps = std::vector<apps::mojom::PreferredAppPtr>;

  // Find preferred app id for an |intent|.
  absl::optional<std::string> FindPreferredAppForIntent(
      const apps::mojom::IntentPtr& intent);

  // Find preferred app id for an |url|.
  absl::optional<std::string> FindPreferredAppForUrl(const GURL& url);

  // Add a preferred app for an |intent_filter|, and returns a group of
  // |app_ids| that is no longer preferred app of their corresponding
  // |intent_filters|.
  apps::mojom::ReplacedAppPreferencesPtr AddPreferredApp(
      const std::string& app_id,
      const apps::mojom::IntentFilterPtr& intent_filter);

  // Delete a preferred app for an |intent_filter| with the same |app_id|.
  // Returns |true| if |app_id| was found in the list of preferred apps.
  bool DeletePreferredApp(const std::string& app_id,
                          const apps::mojom::IntentFilterPtr& intent_filter);

  // Delete all settings for an |app_id|.
  // Returns |true| if |app_id| was found in the list of preferred apps.
  bool DeleteAppId(const std::string& app_id);

  // Initialize the preferred app with empty list or existing |preferred_apps|;
  void Init();
  void Init(PreferredApps& preferred_apps);

  // Get a copy of the preferred apps.
  PreferredApps GetValue();

  bool IsInitialized();

  const PreferredApps& GetReference() const;

  // Get the entry size of the preferred app list.
  size_t GetEntrySize();

  bool IsPreferredAppForSupportedLinks(const std::string& app_id);

 private:
  PreferredApps preferred_apps_;
  base::ObserverList<Observer> observers_;
  bool initialized_ = false;
};

}  // namespace apps

#endif  // COMPONENTS_SERVICES_APP_SERVICE_PUBLIC_CPP_PREFERRED_APPS_LIST_H_
