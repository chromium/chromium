// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/tab_model/tab_model_list.h"

#include <jni.h>

#include "base/android/jni_android.h"
#include "base/ranges/algorithm.h"
#include "chrome/browser/android/tab_android.h"
#include "chrome/browser/ui/android/tab_model/tab_model.h"
#include "chrome/browser/ui/android/tab_model/tab_model_list_observer.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "content/public/browser/web_contents.h"

namespace {
base::LazyInstance<TabModelList>::Leaky tab_model_list_ =
    LAZY_INSTANCE_INITIALIZER;
}  // namespace

static TabModel* archived_tab_model_ = nullptr;

TabModelList::TabModelList() = default;
TabModelList::~TabModelList() = default;

void TabModelList::AddTabModel(TabModel* tab_model) {
  DCHECK(tab_model);
  tab_model_list_.Get().models_.push_back(tab_model);

  for (TabModelListObserver& observer : tab_model_list_.Get().observers_)
    observer.OnTabModelAdded();
}

void TabModelList::RemoveTabModel(TabModel* tab_model) {
  DCHECK(tab_model);
  auto& tab_models = tab_model_list_.Get().models_;

  TabModelList::iterator remove_tab_model =
      base::ranges::find(tab_models, tab_model);

  if (remove_tab_model != tab_models.end())
    tab_models.erase(remove_tab_model);

  for (TabModelListObserver& observer : tab_model_list_.Get().observers_)
    observer.OnTabModelRemoved();
}

void TabModelList::AddObserver(TabModelListObserver* observer) {
  tab_model_list_.Get().observers_.AddObserver(observer);
}

void TabModelList::RemoveObserver(TabModelListObserver* observer) {
  tab_model_list_.Get().observers_.RemoveObserver(observer);
}

void TabModelList::HandlePopupNavigation(NavigateParams* params) {
  TabAndroid* tab = TabAndroid::FromWebContents(params->source_contents);

  // NOTE: If this fails contact dtrainor@.
  DCHECK(tab);
  TabModel* model = FindTabModelWithId(tab->GetWindowId());
  if (model)
    model->HandlePopupNavigation(tab, params);
}

TabModel* TabModelList::GetTabModelForWebContents(
    content::WebContents* web_contents) {
  if (!web_contents)
    return nullptr;

  for (TabModel* model : models()) {
    const size_t tab_count = model->GetTabCount();
    for (size_t index = 0; index < tab_count; index++) {
      if (web_contents == model->GetWebContentsAt(index))
        return model;
    }
  }

  return nullptr;
}

TabModel* TabModelList::GetTabModelForTabAndroid(TabAndroid* tab_android) {
  if (!tab_android)
    return nullptr;

  for (TabModel* model : models()) {
    const size_t tab_count = model->GetTabCount();
    for (size_t index = 0; index < tab_count; index++) {
      if (tab_android == model->GetTabAt(index))
        return model;
    }
  }

  return nullptr;
}

TabModel* TabModelList::FindTabModelWithId(SessionID desired_id) {
  for (TabModel* model : models()) {
    if (model->GetSessionId() == desired_id)
      return model;
  }

  return nullptr;
}

TabModel* TabModelList::FindNativeTabModelForJavaObject(
    const base::android::ScopedJavaLocalRef<jobject>& jtab_model) {
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
    if (model->IsOffTheRecord() && model->GetTabCount() > 0)
      return true;
  }

  return false;
}

// static
const TabModelList::TabModelVector& TabModelList::models() {
  return tab_model_list_.Get().models_;
}

// static
void TabModelList::SetArchivedTabModel(TabModel* archived_tab_model) {
  archived_tab_model_ = archived_tab_model;
}

// static
TabModel* TabModelList::GetArchivedTabModel() {
  return archived_tab_model_;
}
