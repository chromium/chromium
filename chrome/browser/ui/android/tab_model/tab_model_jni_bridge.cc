// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/tab_model/tab_model_jni_bridge.h"

#include <stdint.h>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/android/jni_weak_ref.h"
#include "base/metrics/histogram_macros.h"
#include "base/time/time.h"
#include "chrome/android/chrome_jni_headers/TabModelJniBridge_jni.h"
#include "chrome/browser/android/tab_android.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_android.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/tab_contents/tab_util.h"
#include "chrome/browser/ui/android/tab_model/tab_model_list.h"
#include "chrome/browser/ui/android/tab_model/tab_model_observer_jni_bridge.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/resource_request_body_android.h"
#include "ui/base/window_open_disposition.h"

using base::android::AttachCurrentThread;
using base::android::ConvertUTF8ToJavaString;
using base::android::JavaParamRef;
using base::android::ScopedJavaLocalRef;
using content::WebContents;

namespace {

static Profile* FindProfile(jboolean is_incognito) {
  Profile* profile = ProfileManager::GetActiveUserProfile();
  if (is_incognito)
    return profile->GetOffTheRecordProfile();
  return profile;
}

}  // namespace

TabModelJniBridge::TabModelJniBridge(JNIEnv* env,
                                     jobject jobj,
                                     bool is_incognito,
                                     bool is_tabbed_activity)
    : TabModel(FindProfile(is_incognito), is_tabbed_activity),
      java_object_(env, env->NewWeakGlobalRef(jobj)) {
  TabModelList::AddTabModel(this);
}

void TabModelJniBridge::Destroy(JNIEnv* env, const JavaParamRef<jobject>& obj) {
  delete this;
}

ScopedJavaLocalRef<jobject> TabModelJniBridge::GetProfileAndroid(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) {
  ProfileAndroid* profile_android = ProfileAndroid::FromProfile(GetProfile());
  if (!profile_android)
    return ScopedJavaLocalRef<jobject>();
  return profile_android->GetJavaObject();
}

void TabModelJniBridge::TabAddedToModel(JNIEnv* env,
                                        const JavaParamRef<jobject>& obj,
                                        const JavaParamRef<jobject>& jtab) {
  // Tab#initialize() should have been called by now otherwise we can't push
  // the window id.
  TabAndroid* tab = TabAndroid::GetNativeTab(env, jtab);
  if (tab)
    tab->SetWindowSessionID(GetSessionId());

  if (IsOffTheRecord())
    UMA_HISTOGRAM_COUNTS_100("Tab.Count.Incognito", GetTabCount());
}

int TabModelJniBridge::GetTabCount() const {
  JNIEnv* env = AttachCurrentThread();
  return Java_TabModelJniBridge_getCount(env, java_object_.get(env));
}

int TabModelJniBridge::GetActiveIndex() const {
  JNIEnv* env = AttachCurrentThread();
  return Java_TabModelJniBridge_index(env, java_object_.get(env));
}

void TabModelJniBridge::CreateTab(TabAndroid* parent,
                                  WebContents* web_contents) {
  JNIEnv* env = AttachCurrentThread();
  Java_TabModelJniBridge_createTabWithWebContents(
      env, java_object_.get(env), (parent ? parent->GetJavaObject() : nullptr),
      web_contents->GetBrowserContext()->IsOffTheRecord(),
      web_contents->GetJavaWebContents());
}

void TabModelJniBridge::HandlePopupNavigation(TabAndroid* parent,
                                              NavigateParams* params) {
  DCHECK_EQ(params->source_contents, parent->web_contents());
  DCHECK(!params->contents_to_insert);
  DCHECK(!params->switch_to_singleton_tab);

  WindowOpenDisposition disposition = params->disposition;
  bool supported = disposition == WindowOpenDisposition::NEW_POPUP ||
                   disposition == WindowOpenDisposition::NEW_FOREGROUND_TAB ||
                   disposition == WindowOpenDisposition::NEW_BACKGROUND_TAB ||
                   disposition == WindowOpenDisposition::NEW_WINDOW ||
                   disposition == WindowOpenDisposition::OFF_THE_RECORD;
  if (!supported) {
    NOTIMPLEMENTED();
    return;
  }

  const GURL& url = params->url;
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> jobj = java_object_.get(env);
  ScopedJavaLocalRef<jstring> jurl(ConvertUTF8ToJavaString(env, url.spec()));
  ScopedJavaLocalRef<jstring> jheaders(
      ConvertUTF8ToJavaString(env, params->extra_headers));
  ScopedJavaLocalRef<jstring> jinitiator_origin;
  if (params->initiator_origin) {
    jinitiator_origin =
        ConvertUTF8ToJavaString(env, params->initiator_origin->Serialize());
  }
  ScopedJavaLocalRef<jobject> jpost_data =
      content::ConvertResourceRequestBodyToJavaObject(env, params->post_data);
  Java_TabModelJniBridge_openNewTab(
      env, jobj, parent->GetJavaObject(), jurl, jinitiator_origin, jheaders,
      jpost_data, static_cast<int>(disposition), params->created_with_opener,
      params->is_renderer_initiated);
}

WebContents* TabModelJniBridge::GetWebContentsAt(int index) const {
  TabAndroid* tab = GetTabAt(index);
  return tab == NULL ? NULL : tab->web_contents();
}

TabAndroid* TabModelJniBridge::GetTabAt(int index) const {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> jtab =
      Java_TabModelJniBridge_getTabAt(env, java_object_.get(env), index);

  return jtab.is_null() ? NULL : TabAndroid::GetNativeTab(env, jtab);
}

void TabModelJniBridge::SetActiveIndex(int index) {
  JNIEnv* env = AttachCurrentThread();
  Java_TabModelJniBridge_setIndex(env, java_object_.get(env), index);
}

void TabModelJniBridge::CloseTabAt(int index) {
  JNIEnv* env = AttachCurrentThread();
  Java_TabModelJniBridge_closeTabAt(env, java_object_.get(env), index);
}

WebContents* TabModelJniBridge::CreateNewTabForDevTools(
    const GURL& url) {
  // TODO(dfalcantara): Change the Java side so that it creates and returns the
  //                    WebContents, which we can load the URL on and return.
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jstring> jurl = ConvertUTF8ToJavaString(env, url.spec());
  ScopedJavaLocalRef<jobject> obj =
      Java_TabModelJniBridge_createNewTabForDevTools(env, java_object_.get(env),
                                                     jurl);
  if (obj.is_null()) {
    VLOG(0) << "Failed to create java tab";
    return NULL;
  }
  TabAndroid* tab = TabAndroid::GetNativeTab(env, obj);
  if (!tab) {
    VLOG(0) << "Failed to create java tab";
    return NULL;
  }
  return tab->web_contents();
}

bool TabModelJniBridge::IsSessionRestoreInProgress() const {
  JNIEnv* env = AttachCurrentThread();
  return Java_TabModelJniBridge_isSessionRestoreInProgress(
      env, java_object_.get(env));
}

bool TabModelJniBridge::IsCurrentModel() const {
  JNIEnv* env = AttachCurrentThread();
  return Java_TabModelJniBridge_isCurrentModel(env, java_object_.get(env));
}

void TabModelJniBridge::AddObserver(TabModelObserver* observer) {
  // If a first observer is being added then instantiate an observer bridge.
  if (!observer_bridge_) {
    JNIEnv* env = AttachCurrentThread();
    observer_bridge_ =
        std::make_unique<TabModelObserverJniBridge>(env, java_object_.get(env));
  }
  observer_bridge_->AddObserver(observer);
}

void TabModelJniBridge::RemoveObserver(TabModelObserver* observer) {
  observer_bridge_->RemoveObserver(observer);

  // Tear down the bridge if there are no observers left.
  if (!observer_bridge_->might_have_observers())
    observer_bridge_.reset();
}

void TabModelJniBridge::BroadcastSessionRestoreComplete(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) {
  TabModel::BroadcastSessionRestoreComplete();
}

inline static base::TimeDelta GetTimeDelta(jlong ms) {
  return base::TimeDelta::FromMilliseconds(static_cast<int64_t>(ms));
}

void JNI_TabModelJniBridge_LogFromCloseMetric(
    JNIEnv* env,
    jlong ms,
    jboolean perceived) {
  if (perceived) {
    UMA_HISTOGRAM_TIMES("Tabs.SwitchFromCloseLatency_Perceived",
                        GetTimeDelta(ms));
  } else {
    UMA_HISTOGRAM_TIMES("Tabs.SwitchFromCloseLatency_Actual",
                        GetTimeDelta(ms));
  }
}

void JNI_TabModelJniBridge_LogFromExitMetric(
    JNIEnv* env,
    jlong ms,
    jboolean perceived) {
  if (perceived) {
    UMA_HISTOGRAM_TIMES("Tabs.SwitchFromExitLatency_Perceived",
                        GetTimeDelta(ms));
  } else {
    UMA_HISTOGRAM_TIMES("Tabs.SwitchFromExitLatency_Actual",
                        GetTimeDelta(ms));
  }
}

void JNI_TabModelJniBridge_LogFromNewMetric(JNIEnv* env,
                                            jlong ms,
                                            jboolean perceived) {
  if (perceived) {
    UMA_HISTOGRAM_TIMES("Tabs.SwitchFromNewLatency_Perceived",
                        GetTimeDelta(ms));
  } else {
    UMA_HISTOGRAM_TIMES("Tabs.SwitchFromNewLatency_Actual",
                        GetTimeDelta(ms));
  }
}

void JNI_TabModelJniBridge_LogFromUserMetric(
    JNIEnv* env,
    jlong ms,
    jboolean perceived) {
  if (perceived) {
    UMA_HISTOGRAM_TIMES("Tabs.SwitchFromUserLatency_Perceived",
                        GetTimeDelta(ms));
  } else {
    UMA_HISTOGRAM_TIMES("Tabs.SwitchFromUserLatency_Actual",
                        GetTimeDelta(ms));
  }
}

TabModelJniBridge::~TabModelJniBridge() {
  TabModelList::RemoveTabModel(this);
}

static jlong JNI_TabModelJniBridge_Init(JNIEnv* env,
                                        const JavaParamRef<jobject>& obj,
                                        jboolean is_incognito,
                                        jboolean is_tabbed_activity) {
  TabModel* tab_model =
      new TabModelJniBridge(env, obj, is_incognito, is_tabbed_activity);
  return reinterpret_cast<intptr_t>(tab_model);
}
