// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ANDROID_TAB_MODEL_TAB_MODEL_LIST_H_
#define CHROME_BROWSER_UI_ANDROID_TAB_MODEL_TAB_MODEL_LIST_H_

#include <vector>

#include "base/functional/function_ref.h"
#include "base/memory/raw_ptr.h"
#include "components/sessions/core/session_id.h"

class Profile;
class TabAndroid;
class TabModel;
class TabModelListObserver;

struct NavigateParams;

namespace content {
class WebContents;
}

// Stores a list of all TabModel objects, partitioned by Profile.
// Must only be accessed on the UI thread (or on the main thread before
// `BrowserThread::UI` is initialized).
class TabModelList {
 public:
  using TabModelVector = std::vector<raw_ptr<TabModel, VectorExperimental>>;

  TabModelList() = delete;
  TabModelList(const TabModelList& other) = delete;
  TabModelList& operator=(const TabModelList& other) = delete;
  ~TabModelList() = delete;

  static void HandlePopupNavigation(NavigateParams* params);

  // Registered TabModels should be removed via RemoveTabModel before their
  // Profile is destroyed.
  static void AddTabModel(TabModel* tab_model);
  static void RemoveTabModel(TabModel* tab_model);

  // Profile-scoped iteration and observer APIs. Matches by exact `Profile*`
  // (an off-the-record `Profile*` is distinct from its original regular
  // `Profile*`).
  //
  // `ForEach` enumerates all registered `TabModel`s belonging to `profile` in
  // insertion order, including Custom Tab and headless models (archived
  // `TabModel`s are tracked separately via `GetArchivedTabModel`), so callers
  // that only want standard tabbed models should filter by
  // `TabModel::activity_type()` / `GetTabModelType()`. Returning `false` from
  // `on_tab_model` stops iteration early, and iteration is safe against
  // re-entrant `AddTabModel` and `RemoveTabModel` calls.
  //
  // `observer` and `profile` must be non-null for `AddObserver` and
  // `RemoveObserver`, and observers must be unregistered via `RemoveObserver`
  // before `profile` is destroyed (`ForEach` and `GetArchivedTabModel` return
  // empty / `nullptr` when `profile` is `nullptr`; passing a dangling
  // `Profile*` after `~Profile()` is unsupported). An observer added during an
  // in-progress `OnTabModelAdded` / `OnTabModelRemoved` notification does not
  // receive that notification, so callers initializing state should call
  // `AddObserver` first and then `ForEach` to seed existing models without
  // duplicates.
  static void ForEach(const Profile* profile,
                      base::FunctionRef<bool(TabModel*)> on_tab_model);
  static void AddObserver(TabModelListObserver* observer, Profile* profile);
  static void RemoveObserver(TabModelListObserver* observer,
                             const Profile* profile);

  // Browser-global iteration, query, and observer APIs spanning all profiles.
  // Prefer the profile-scoped APIs above unless cross-profile behavior is
  // explicitly required.
  static void ForEachAll(base::FunctionRef<bool(TabModel*)> on_tab_model);
  static bool Contains(const TabModel* tab_model);
  static void AddGlobalObserver(TabModelListObserver* observer);
  static void RemoveGlobalObserver(TabModelListObserver* observer);

  // TODO(crbug.com/580974657): Migrate callers to profile-keyed
  // `AddObserver(observer, profile)` / `RemoveObserver(observer, profile)` or
  // explicit `AddGlobalObserver` / `RemoveGlobalObserver`.
  static void AddObserver(TabModelListObserver* observer);
  static void RemoveObserver(TabModelListObserver* observer);

  static TabModel* GetTabModelForWebContents(
      content::WebContents* web_contents);
  static TabModel* GetTabModelForTabAndroid(TabAndroid* tab_android);
  static TabModel* FindTabModelWithWindowSessionId(SessionID desired_id);
  static bool IsOffTheRecordSessionActive();

  // TODO(crbug.com/580974657): Migrate callers to `ForEach(profile, ...)` or
  // `ForEachAll(...)`.
  static const TabModelVector& models();

  static void SetArchivedTabModel(TabModel* archived_tab_model);
  static void RemoveArchivedTabModel(TabModel* archived_tab_model);
  static TabModel* GetArchivedTabModel(const Profile* profile);
};

#endif  // CHROME_BROWSER_UI_ANDROID_TAB_MODEL_TAB_MODEL_LIST_H_
