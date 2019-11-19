// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/tab_model/tab_model_list.h"

#include "base/no_destructor.h"
#include "chrome/browser/android/tab_android.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/android/tab_model/tab_model.h"
#include "chrome/browser/ui/android/tab_model/tab_model_list_observer.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "content/public/browser/web_contents.h"

namespace {

// Maintains and gives access to a static list of TabModel instances.
TabModelList::TabModelVector& tab_models() {
  static base::NoDestructor<TabModelList::TabModelVector> tab_model_vector;
  return *tab_model_vector;
}

}  // namespace

// static
base::LazyInstance<base::ObserverList<TabModelListObserver>::Unchecked>::Leaky
    TabModelList::observers_ = LAZY_INSTANCE_INITIALIZER;

void TabModelList::AddTabModel(TabModel* tab_model) {
  DCHECK(tab_model);
  tab_models().push_back(tab_model);

  for (TabModelListObserver& observer : observers_.Get())
    observer.OnTabModelAdded();
}

void TabModelList::RemoveTabModel(TabModel* tab_model) {
  DCHECK(tab_model);
  TabModelList::iterator remove_tab_model =
      std::find(tab_models().begin(), tab_models().end(), tab_model);

  if (remove_tab_model != tab_models().end())
    tab_models().erase(remove_tab_model);

  for (TabModelListObserver& observer : observers_.Get())
    observer.OnTabModelRemoved();
}

void TabModelList::AddObserver(TabModelListObserver* observer) {
  observers_.Get().AddObserver(observer);
}

void TabModelList::RemoveObserver(TabModelListObserver* observer) {
  observers_.Get().RemoveObserver(observer);
}

void TabModelList::HandlePopupNavigation(NavigateParams* params) {
  TabAndroid* tab = TabAndroid::FromWebContents(params->source_contents);

  // NOTE: If this fails contact dtrainor@.
  DCHECK(tab);
  TabModel* model = FindTabModelWithId(tab->window_id());
  if (model)
    model->HandlePopupNavigation(tab, params);
}

TabModel* TabModelList::GetTabModelForWebContents(
    content::WebContents* web_contents) {
  if (!web_contents)
    return NULL;

  for (TabModelList::const_iterator i = TabModelList::begin();
      i != TabModelList::end(); ++i) {
    TabModel* model = *i;
    for (int index = 0; index < model->GetTabCount(); index++) {
      if (web_contents == model->GetWebContentsAt(index))
        return model;
    }
  }

  return NULL;
}

TabModel* TabModelList::FindTabModelWithId(SessionID desired_id) {
  for (TabModelList::const_iterator i = TabModelList::begin();
      i != TabModelList::end(); i++) {
    if ((*i)->GetSessionId() == desired_id)
      return *i;
  }

  return NULL;
}

bool TabModelList::IsOffTheRecordSessionActive() {
  for (TabModelList::const_iterator i = TabModelList::begin();
      i != TabModelList::end(); i++) {
    if ((*i)->IsOffTheRecord() && (*i)->GetTabCount() > 0)
      return true;
  }

  return false;
}

TabModelList::const_iterator TabModelList::begin() {
  return tab_models().begin();
}

TabModelList::const_iterator TabModelList::end() {
  return tab_models().end();
}

bool TabModelList::empty() {
  return tab_models().empty();
}

size_t TabModelList::size() {
  return tab_models().size();
}

TabModel* TabModelList::get(size_t index) {
  DCHECK_LT(index, size());
  return tab_models()[index];
}
