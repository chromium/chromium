// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ANDROID_TAB_MODEL_TAB_MODEL_LIST_H_
#define CHROME_BROWSER_UI_ANDROID_TAB_MODEL_TAB_MODEL_LIST_H_

#include <stddef.h>

#include <vector>

#include "base/android/scoped_java_ref.h"
#include "base/lazy_instance.h"
#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "components/sessions/core/session_id.h"

class TabAndroid;
class TabModel;
class TabModelListObserver;

struct NavigateParams;

namespace content {
class WebContents;
}

// Stores a list of all TabModel objects.
class TabModelList {
 public:
  typedef std::vector<raw_ptr<TabModel, VectorExperimental>> TabModelVector;
  typedef TabModelVector::iterator iterator;
  typedef TabModelVector::const_iterator const_iterator;

  TabModelList(const TabModelList& other) = delete;
  TabModelList(TabModelList&& other) = delete;
  TabModelList& operator=(const TabModelList& other) = delete;
  TabModelList&& operator=(TabModelList&& other) = delete;
  ~TabModelList();

  static void HandlePopupNavigation(NavigateParams* params);
  static void AddTabModel(TabModel* tab_model);
  static void RemoveTabModel(TabModel* tab_model);

  static void AddObserver(TabModelListObserver* observer);
  static void RemoveObserver(TabModelListObserver* observer);

  static TabModel* GetTabModelForWebContents(
      content::WebContents* web_contents);
  static TabModel* GetTabModelForTabAndroid(TabAndroid* tab_android);
  static TabModel* FindTabModelWithId(SessionID desired_id);
  static TabModel* FindNativeTabModelForJavaObject(
      const base::android::ScopedJavaLocalRef<jobject>& jtab_model);
  static bool IsOffTheRecordSessionActive();

  static const TabModelVector& models();

  static void SetArchivedTabModel(TabModel* archived_tab_model);
  static TabModel* GetArchivedTabModel();

 private:
  TabModelList();

  // A list of observers which will be notified of every TabModel addition and
  // removal across all TabModelLists.
  base::ObserverList<TabModelListObserver>::Unchecked observers_;
  TabModelVector models_;

  friend base::LazyInstanceTraitsBase<TabModelList>;
};

#endif  // CHROME_BROWSER_UI_ANDROID_TAB_MODEL_TAB_MODEL_LIST_H_
