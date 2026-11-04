// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/tab_model/tab_model_list.h"

#include <algorithm>
#include <cstdint>
#include <memory>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/containers/flat_map.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/no_destructor.h"
#include "base/observer_list.h"
#include "base/supports_user_data.h"
#include "chrome/browser/android/tab_android.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/android/tab_model/tab_model.h"
#include "chrome/browser/ui/android/tab_model/tab_model_list_observer.h"
#include "chrome/browser/ui/navigator/browser_navigator_params.h"
#include "components/sessions/core/session_id.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"

namespace {

using GlobalObserverList =
    base::ObserverList<TabModelListObserver, /*check_empty=*/false>::Unchecked;

using ProfileObserverList =
    base::ObserverList<TabModelListObserver, /*check_empty=*/true>::Unchecked;

const int kProfileTabModelDataKey = 0;

void AssertOnUiThread() {
  // Early startup callers (such as `ChromeTracingDelegate`) register global
  // observers on the main thread before `BrowserThread::UI` is registered, and
  // unit tests may tear down models or global observers after
  // `BrowserTaskEnvironment` destruction.
  DCHECK(!content::BrowserThread::IsThreadInitialized(
             content::BrowserThread::UI) ||
         content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
}

struct ProfileTabModelData;

struct TabModelListStorage {
  TabModelList::TabModelVector models;
  GlobalObserverList global_observers{base::ObserverListPolicy::EXISTING_ONLY};
  std::vector<raw_ptr<ProfileTabModelData>> active_profile_data;
};

TabModelListStorage& GetStorage() {
  static base::NoDestructor<TabModelListStorage> storage;
  return *storage;
}

struct ProfileTabModelData : public base::SupportsUserData::Data {
  explicit ProfileTabModelData(const Profile* profile)
      : profile_key(reinterpret_cast<std::uintptr_t>(profile)) {
    GetStorage().active_profile_data.push_back(this);
  }
  ProfileTabModelData(const ProfileTabModelData&) = delete;
  ProfileTabModelData& operator=(const ProfileTabModelData&) = delete;
  ~ProfileTabModelData() override {
    std::erase(GetStorage().active_profile_data, this);
  }

  // Scans `active_profile_data` by `std::uintptr_t` instead of calling virtual
  // `profile->GetUserData(&kProfileTabModelDataKey)`: `~ProfileObserverList()`
  // uses `DUMP_WILL_BE_CHECK(observers_.empty())` (non-fatal in release
  // builds), so avoiding a virtual `GetUserData()` call prevents an immediate
  // vtable UAF if a buggy caller unregisters after `~Profile()` (callers must
  // still unregister before `~Profile()`, as `uintptr_t` keys cannot guard
  // against allocator address reuse).
  static ProfileTabModelData* Get(const Profile* profile) {
    if (!profile) {
      return nullptr;
    }
    const auto key = reinterpret_cast<std::uintptr_t>(profile);
    for (ProfileTabModelData* data : GetStorage().active_profile_data) {
      if (data->profile_key == key) {
        return data;
      }
    }
    return nullptr;
  }

  static ProfileTabModelData& GetOrCreate(Profile* profile) {
    CHECK(profile);
    if (auto* existing = Get(profile)) {
      return *existing;
    }
    auto data = std::make_unique<ProfileTabModelData>(profile);
    auto* ptr = data.get();
    profile->SetUserData(&kProfileTabModelDataKey, std::move(data));
    return *ptr;
  }

  const std::uintptr_t profile_key;
  TabModelList::TabModelVector models;
  ProfileObserverList observers{base::ObserverListPolicy::EXISTING_ONLY};
  // Monotonic registration epoch per observer so profile observers added during
  // `global_observers` dispatch have `epoch >= snapshot_epoch` and are skipped.
  base::flat_map<std::uintptr_t, uint64_t> observer_epochs;
  uint64_t next_observer_epoch = 0;
  raw_ptr<TabModel> archived_tab_model = nullptr;
  base::WeakPtrFactory<ProfileTabModelData> weak_factory{this};
};

// Iterates through `models` present at construction time, skipping any
// `TabModel` removed mid-iteration.
class TabModelListEnumerator : public TabModelListObserver {
 public:
  explicit TabModelListEnumerator(TabModelList::TabModelVector models)
      : models_(std::move(models)) {
    TabModelList::AddGlobalObserver(this);
  }
  TabModelListEnumerator(const TabModelListEnumerator&) = delete;
  TabModelListEnumerator& operator=(const TabModelListEnumerator&) = delete;
  ~TabModelListEnumerator() override {
    TabModelList::RemoveGlobalObserver(this);
  }

  void ForEach(base::FunctionRef<bool(TabModel*)> on_tab_model) {
    for (TabModel* model : models_) {
      if (model && !on_tab_model(model)) {
        return;
      }
    }
  }

 private:
  // TabModelListObserver:
  void OnTabModelAdded(TabModel* tab_model) override {}
  void OnTabModelRemoved(TabModel* tab_model) override {
    auto it = std::ranges::find(models_, tab_model);
    if (it != models_.end()) {
      *it = nullptr;
    }
  }

  TabModelList::TabModelVector models_;
};

void NotifyObservers(TabModel* tab_model,
                     base::WeakPtr<ProfileTabModelData> profile_data,
                     uint64_t snapshot_epoch,
                     void (TabModelListObserver::*notify_method)(TabModel*)) {
  for (auto& observer : GetStorage().global_observers) {
    (observer.*notify_method)(tab_model);
  }
  if (!profile_data) {
    return;
  }
  for (auto& observer : profile_data->observers) {
    const auto key = reinterpret_cast<std::uintptr_t>(&observer);
    auto it = profile_data->observer_epochs.find(key);
    if (it != profile_data->observer_epochs.end() &&
        it->second < snapshot_epoch) {
      (observer.*notify_method)(tab_model);
      if (!profile_data) {
        return;
      }
    }
  }
}

}  // namespace

void TabModelList::AddTabModel(TabModel* tab_model) {
  AssertOnUiThread();
  CHECK(tab_model);
  CHECK(!Contains(tab_model));
  Profile* profile = tab_model->GetProfile();
  CHECK(profile);

  auto& storage = GetStorage();
  storage.models.push_back(tab_model);
  auto& data = ProfileTabModelData::GetOrCreate(profile);
  data.models.push_back(tab_model);
  const uint64_t snapshot_epoch = data.next_observer_epoch;
  base::WeakPtr<ProfileTabModelData> profile_data =
      data.weak_factory.GetWeakPtr();

  NotifyObservers(tab_model, std::move(profile_data), snapshot_epoch,
                  &TabModelListObserver::OnTabModelAdded);
}

void TabModelList::RemoveTabModel(TabModel* tab_model) {
  AssertOnUiThread();
  CHECK(tab_model);
  RemoveArchivedTabModel(tab_model);

  auto& storage = GetStorage();
  if (std::erase(storage.models, tab_model) == 0) {
    return;
  }

  base::WeakPtr<ProfileTabModelData> profile_data;
  uint64_t snapshot_epoch = 0;
  for (ProfileTabModelData* data : storage.active_profile_data) {
    if (std::erase(data->models, tab_model) > 0) {
      snapshot_epoch = data->next_observer_epoch;
      profile_data = data->weak_factory.GetWeakPtr();
      break;
    }
  }

  NotifyObservers(tab_model, std::move(profile_data), snapshot_epoch,
                  &TabModelListObserver::OnTabModelRemoved);
}

void TabModelList::ForEach(const Profile* profile,
                           base::FunctionRef<bool(TabModel*)> on_tab_model) {
  AssertOnUiThread();
  auto* data = ProfileTabModelData::Get(profile);
  if (!data || data->models.empty()) {
    return;
  }
  TabModelListEnumerator(data->models).ForEach(on_tab_model);
}

void TabModelList::AddObserver(TabModelListObserver* observer,
                               Profile* profile) {
  AssertOnUiThread();
  CHECK(observer);
  CHECK(profile);
  auto& data = ProfileTabModelData::GetOrCreate(profile);
  CHECK(!data.observers.HasObserver(observer));
  data.observer_epochs[reinterpret_cast<std::uintptr_t>(observer)] =
      data.next_observer_epoch++;
  data.observers.AddObserver(observer);
}

void TabModelList::RemoveObserver(TabModelListObserver* observer,
                                  const Profile* profile) {
  AssertOnUiThread();
  CHECK(observer);
  CHECK(profile);
  if (auto* data = ProfileTabModelData::Get(profile)) {
    data->observers.RemoveObserver(observer);
    data->observer_epochs.erase(reinterpret_cast<std::uintptr_t>(observer));
  }
}

void TabModelList::ForEachAll(base::FunctionRef<bool(TabModel*)> on_tab_model) {
  AssertOnUiThread();
  const auto& models = GetStorage().models;
  if (models.empty()) {
    return;
  }
  TabModelListEnumerator(models).ForEach(on_tab_model);
}

bool TabModelList::Contains(const TabModel* tab_model) {
  AssertOnUiThread();
  return tab_model && std::ranges::contains(GetStorage().models, tab_model);
}

void TabModelList::AddGlobalObserver(TabModelListObserver* observer) {
  AssertOnUiThread();
  CHECK(observer);
  GetStorage().global_observers.AddObserver(observer);
}

void TabModelList::RemoveGlobalObserver(TabModelListObserver* observer) {
  AssertOnUiThread();
  CHECK(observer);
  GetStorage().global_observers.RemoveObserver(observer);
}

void TabModelList::AddObserver(TabModelListObserver* observer) {
  AddGlobalObserver(observer);
}

void TabModelList::RemoveObserver(TabModelListObserver* observer) {
  RemoveGlobalObserver(observer);
}

void TabModelList::HandlePopupNavigation(NavigateParams* params) {
  AssertOnUiThread();
  TabAndroid* tab = TabAndroid::FromWebContents(params->source_contents);

  // NOTE: If this fails contact dtrainor@.
  CHECK(tab, base::NotFatalUntil::M162);
  if (!tab) {
    return;
  }
  // Prefer membership lookup as unassociated TabModels lack a valid SessionID.
  TabModel* model = GetTabModelForTabAndroid(tab);
  if (!model && tab->GetWindowId().is_valid()) {
    model = FindTabModelWithWindowSessionId(tab->GetWindowId());
  }
  if (model) {
    model->HandlePopupNavigation(tab, params);
  }
}

TabModel* TabModelList::GetTabModelForWebContents(
    content::WebContents* web_contents) {
  AssertOnUiThread();
  if (!web_contents) {
    return nullptr;
  }

  TabAndroid* tab_android = TabAndroid::FromWebContents(web_contents);
  if (tab_android) {
    TabModel* model = GetTabModelForTabAndroid(tab_android);
    if (model) {
      return model;
    }
  }

  // Fallback for tests or custom TabModel implementations (e.g. TestTabModel)
  // where TabAndroid is not attached to WebContents.
  for (TabModel* model : GetStorage().models) {
    for (int i = 0; i < model->GetTabCount(); ++i) {
      if (model->GetWebContentsAt(i) == web_contents) {
        return model;
      }
    }
  }

  return nullptr;
}

TabModel* TabModelList::GetTabModelForTabAndroid(TabAndroid* tab_android) {
  AssertOnUiThread();
  if (!tab_android) {
    return nullptr;
  }

  for (TabModel* model : GetStorage().models) {
    if (model->HasTab(tab_android)) {
      return model;
    }
  }

  return nullptr;
}

TabModel* TabModelList::FindTabModelWithWindowSessionId(SessionID desired_id) {
  AssertOnUiThread();
  const auto& models = GetStorage().models;
  auto it = std::ranges::find_if(models, [desired_id](const TabModel* model) {
    return model->GetSessionId() == desired_id;
  });

  return it != models.end() ? *it : nullptr;
}

bool TabModelList::IsOffTheRecordSessionActive() {
  AssertOnUiThread();
  // TODO(crbug.com/40107157): This function should return true for
  // incognito CCTs.
  return std::ranges::any_of(GetStorage().models, [](const TabModel* model) {
    return model->IsOffTheRecord() && model->GetTabCount() > 0;
  });
}

// static
const TabModelList::TabModelVector& TabModelList::models() {
  AssertOnUiThread();
  return GetStorage().models;
}

// static
void TabModelList::SetArchivedTabModel(TabModel* archived_tab_model) {
  AssertOnUiThread();
  CHECK(archived_tab_model);
  Profile* profile = archived_tab_model->GetProfile();
  CHECK(profile);
  ProfileTabModelData::GetOrCreate(profile).archived_tab_model =
      archived_tab_model;
}

// static
void TabModelList::RemoveArchivedTabModel(TabModel* archived_tab_model) {
  AssertOnUiThread();
  CHECK(archived_tab_model);
  for (ProfileTabModelData* data : GetStorage().active_profile_data) {
    if (data->archived_tab_model == archived_tab_model) {
      data->archived_tab_model = nullptr;
    }
  }
}

// static
TabModel* TabModelList::GetArchivedTabModel(const Profile* profile) {
  AssertOnUiThread();
  auto* data = ProfileTabModelData::Get(profile);
  return data ? data->archived_tab_model.get() : nullptr;
}
