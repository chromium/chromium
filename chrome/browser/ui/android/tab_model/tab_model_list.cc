// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/tab_model/tab_model_list.h"

#include <algorithm>

#include "chrome/browser/android/tab_android.h"
#include "chrome/browser/ui/android/tab_model/tab_model.h"
#include "chrome/browser/ui/android/tab_model/tab_model_list_observer.h"
#include "chrome/browser/ui/navigator/browser_navigator_params.h"
#include "components/sessions/core/session_id.h"
#include "content/public/browser/web_contents.h"

namespace {
TabModelList& GetInstance() {
  static base::NoDestructor<TabModelList> tab_model_list;
  return *tab_model_list;
}
}  // namespace

TabModelList::TabModelList() = default;
TabModelList::~TabModelList() = default;

void TabModelList::AddTabModel(TabModel* tab_model) {
  CHECK(tab_model, base::NotFatalUntil::M162);
  GetInstance().models_.push_back(tab_model);

  for (TabModelListObserver& observer : GetInstance().observers_) {
    observer.OnTabModelAdded(tab_model);
  }
}

void TabModelList::RemoveTabModel(TabModel* tab_model) {
  CHECK(tab_model, base::NotFatalUntil::M162);
  RemoveArchivedTabModel(tab_model);
  auto& tab_models = GetInstance().models_;
  std::erase(tab_models, tab_model);

  for (TabModelListObserver& observer : GetInstance().observers_) {
    observer.OnTabModelRemoved(tab_model);
  }
}

void TabModelList::AddObserver(TabModelListObserver* observer) {
  GetInstance().observers_.AddObserver(observer);
}

void TabModelList::RemoveObserver(TabModelListObserver* observer) {
  GetInstance().observers_.RemoveObserver(observer);
}

void TabModelList::HandlePopupNavigation(NavigateParams* params) {
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
  for (TabModel* model : models()) {
    for (int i = 0; i < model->GetTabCount(); ++i) {
      if (model->GetWebContentsAt(i) == web_contents) {
        return model;
      }
    }
  }

  return nullptr;
}

TabModel* TabModelList::GetTabModelForTabAndroid(TabAndroid* tab_android) {
  if (!tab_android) {
    return nullptr;
  }

  for (TabModel* model : models()) {
    if (model->HasTab(tab_android)) {
      return model;
    }
  }

  return nullptr;
}

TabModel* TabModelList::FindTabModelWithWindowSessionId(SessionID desired_id) {
  auto it = std::ranges::find_if(models(), [desired_id](const TabModel* model) {
    return model->GetSessionId() == desired_id;
  });

  return it != models().end() ? *it : nullptr;
}

bool TabModelList::IsOffTheRecordSessionActive() {
  // TODO(crbug.com/40107157): This function should return true for
  // incognito CCTs.
  return std::ranges::any_of(models(), [](const TabModel* model) {
    return model->IsOffTheRecord() && model->GetTabCount() > 0;
  });
}

// static
const TabModelList::TabModelVector& TabModelList::models() {
  return GetInstance().models_;
}

// static
void TabModelList::SetArchivedTabModel(TabModel* archived_tab_model) {
  CHECK(archived_tab_model);
  CHECK(archived_tab_model->GetProfile());
  GetInstance().archived_tab_models_[archived_tab_model->GetProfile()] =
      archived_tab_model;
}

// static
void TabModelList::RemoveArchivedTabModel(TabModel* archived_tab_model) {
  CHECK(archived_tab_model);
  auto& archived_models = GetInstance().archived_tab_models_;
  auto it = archived_models.find(archived_tab_model->GetProfile());
  if (it != archived_models.end() && it->second == archived_tab_model) {
    archived_models.erase(it);
  }
}

// static
TabModel* TabModelList::GetArchivedTabModel(Profile* profile) {
  if (!profile) {
    return nullptr;
  }
  auto& archived_models = GetInstance().archived_tab_models_;
  auto it = archived_models.find(profile);
  return it != archived_models.end() ? it->second.get() : nullptr;
}
