// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SERVICES_APP_SERVICE_PUBLIC_CPP_PREFERRED_APPS_LIST_H_
#define COMPONENTS_SERVICES_APP_SERVICE_PUBLIC_CPP_PREFERRED_APPS_LIST_H_

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/containers/flat_set.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "components/services/app_service/public/cpp/intent.h"
#include "components/services/app_service/public/cpp/intent_filter.h"
#include "components/services/app_service/public/cpp/preferred_app.h"
#include "components/services/app_service/public/cpp/preferred_apps_list_handle.h"

class GURL;

namespace apps {

// The preferred apps set by the user. The preferred apps is stored as
// an list of |intent_filter| vs. app_id.
class PreferredAppsList : public PreferredAppsListHandle {
 public:
  class Delegate {
   public:
    // Returns true if two structurally overlapping preferred app filter
    // configurations conflict, indicating that they cannot co-exist and the
    // older preference must be disabled. If the delegate returns false, they
    // can co-exist (e.g., nested scopes of different non-system web apps).
    virtual bool QueryConflict(const std::string& first_app_id,
                               const IntentFilterPtr& first_filter,
                               const std::string& second_app_id,
                               const IntentFilterPtr& second_filter) = 0;

    // Returns true if the given url falls within the scope extensions of the
    // specified web app. This is used to deprioritize extended scope matches
    // in favor of web app scope matches.
    virtual bool IsWebAppInExtendedScope(const GURL& url,
                                         const std::string& app_id) const = 0;

   protected:
    virtual ~Delegate() = default;
  };

  explicit PreferredAppsList(Delegate* delegate);
  ~PreferredAppsList();

  PreferredAppsList(const PreferredAppsList&) = delete;
  PreferredAppsList& operator=(const PreferredAppsList&) = delete;

  // Initialize the preferred app with empty list or existing |preferred_apps|;
  void Init();
  void Init(PreferredApps preferred_apps);

  void SetLongestPrefixMatchEnabled(bool enabled) {
    longest_prefix_match_enabled_ = enabled;
  }

  // Add a preferred app for an |intent_filter|, and returns a group of
  // |app_ids| that is no longer preferred app of their corresponding
  // |intent_filters|.
  ReplacedAppPreferences AddPreferredApp(const std::string& app_id,
                                         const IntentFilterPtr& intent_filter);

  // Delete a preferred app for an |intent_filter| with the same |app_id|.
  // Returns the deleted filters, if any.
  IntentFilters DeletePreferredApp(const std::string& app_id,
                                   const IntentFilterPtr& intent_filter);

  // Delete all settings for an |app_id|. Returns the deleted filters, if any.
  IntentFilters DeleteAppId(const std::string& app_id);

  // Deletes all stored supported link preferences for an |app_id|.
  // Returns the deleted filters, if any.
  IntentFilters DeleteSupportedLinks(const std::string& app_id);

  // PreferredAppsListHandler overrides:
  bool IsInitialized() const override;
  size_t GetEntrySize() const override;
  PreferredApps GetValue() const override;
  const PreferredApps& GetReference() const override;
  bool IsPreferredAppForSupportedLinks(
      const std::string& app_id) const override;
  std::optional<std::string> FindPreferredAppForUrl(
      const GURL& url) const override;
  std::optional<std::string> FindPreferredAppForIntent(
      const IntentPtr& intent) const override;
  base::flat_set<std::string> FindPreferredAppsForFilters(
      std::optional<std::string> app_id,
      const IntentFilters& intent_filters) const override;

 private:
  // Check if the entry already exists in the preferred app list.
  bool EntryExists(const std::string& app_id,
                   const IntentFilterPtr& intent_filter);

  PreferredApps preferred_apps_;
  // The delegate is owned by the same class (PreferredAppsImpl) that owns this
  // PreferredAppsList instance, which guarantees that the delegate will outlive
  // this list and therefore the pointer will never dangle.
  raw_ptr<Delegate> delegate_ = nullptr;
  bool longest_prefix_match_enabled_ = false;
  bool initialized_ = false;
};

}  // namespace apps

#endif  // COMPONENTS_SERVICES_APP_SERVICE_PUBLIC_CPP_PREFERRED_APPS_LIST_H_
