// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/tab_model/tab_model_list.h"

#include <jni.h>

#include <algorithm>

#include "base/android/jni_android.h"
#include "chrome/browser/android/tab_android.h"
#include "chrome/browser/ui/android/tab_model/tab_model.h"
#include "chrome/browser/ui/android/tab_model/tab_model_list_observer.h"
#include "chrome/browser/ui/navigator/browser_navigator_params.h"
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
  DCHECK(tab_model);
  GetInstance().models_.push_back(tab_model);

  for (TabModelListObserver& observer : GetInstance().observers_) {
    observer.OnTabModelAdded(tab_model);
  }
}

void TabModelList::RemoveTabModel(TabModel* tab_model) {
  DCHECK(tab_model);
  if (GetInstance().archived_tab_model_ == tab_model) {
    GetInstance().archived_tab_model_ = nullptr;
  }
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
  DCHECK(tab);
  TabModel* model = FindTabModelWithWindowSessionId(tab->GetWindowId());
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

TabModel* TabModelList::FindNativeTabModelForJavaObject(
    const base::android::JavaRef<jobject>& jtab_model) {
  if (!jtab_model) {
    return nullptr;
  }

  JNIEnv* env = base::android::AttachCurrentThread();
  for (TabModel* model : models()) {
    if (env->IsSameObject(jtab_model.obj(), model->GetJavaObject().obj())) {
      return model;
    }
  }

  TabModel* archived_tab_model = GetArchivedTabModel();
  if (archived_tab_model &&
      env->IsSameObject(jtab_model.obj(),
                        archived_tab_model->GetJavaObject().obj())) {
    return archived_tab_model;
  }

  return nullptr;
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
  GetInstance().archived_tab_model_ = archived_tab_model;
}

// static
TabModel* TabModelList::GetArchivedTabModel() {
  return GetInstance().archived_tab_model_;
}
