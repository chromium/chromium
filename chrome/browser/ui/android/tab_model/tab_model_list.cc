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
#include "chrome/browser/ui/browser_navigator_params.h"
#include "content/public/browser/web_contents.h"

namespace {
TabModelList& GetInstance() {
  static base::NoDestructor<TabModelList> tab_model_list;
  return *tab_model_list;
}
}  // namespace

static TabModel* archived_tab_model_ = nullptr;

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
  auto& tab_models = GetInstance().models_;

  TabModelList::iterator remove_tab_model =
      std::ranges::find(tab_models, tab_model);

  if (remove_tab_model != tab_models.end()) {
    tab_models.erase(remove_tab_model);
  }

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

  for (TabModel* model : models()) {
    const size_t tab_count = model->GetTabCount();
    for (size_t index = 0; index < tab_count; index++) {
      if (web_contents == model->GetWebContentsAt(index)) {
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
    const size_t tab_count = model->GetTabCount();
    for (size_t index = 0; index < tab_count; index++) {
      if (tab_android == model->GetTabAt(index)) {
        return model;
      }
    }
  }

  return nullptr;
}

TabModel* TabModelList::FindTabModelWithWindowSessionId(SessionID desired_id) {
  for (TabModel* model : models()) {
    if (model->GetSessionId() == desired_id) {
      return model;
    }
  }

  return nullptr;
}

TabModel* TabModelList::FindNativeTabModelForJavaObject(
    const base::android::JavaRef<jobject>& jtab_model) {
  JNIEnv* env = base::android::AttachCurrentThread();
  for (TabModel* model : models()) {
    if (env->IsSameObject(jtab_model.obj(), model->GetJavaObject().obj())) {
      return model;
    }
  }

  TabModel* archived_tab_model = GetArchivedTabModel();
  if (archived_tab_model != nullptr &&
      env->IsSameObject(jtab_model.obj(),
                        archived_tab_model->GetJavaObject().obj())) {
    return archived_tab_model;
  }

  return nullptr;
}

bool TabModelList::IsOffTheRecordSessionActive() {
  // TODO(crbug.com/40107157): This function should return true for
  // incognito CCTs.
  for (TabModel* model : models()) {
    if (model->IsOffTheRecord() && model->GetTabCount() > 0) {
      return true;
    }
  }

  return false;
}

// static
const TabModelList::TabModelVector& TabModelList::models() {
  return GetInstance().models_;
}

// static
void TabModelList::SetArchivedTabModel(TabModel* archived_tab_model) {
  archived_tab_model_ = archived_tab_model;
}

// static
TabModel* TabModelList::GetArchivedTabModel() {
  return archived_tab_model_;
}
