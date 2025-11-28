// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/tab_model/tab_model_jni_bridge.h"

#include <jni.h>
#include <stdint.h>

#include <set>
#include <utility>

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/android/jni_weak_ref.h"
#include "base/android/token_android.h"
#include "base/metrics/histogram_macros.h"
#include "base/notimplemented.h"
#include "base/time/time.h"
#include "base/token.h"
#include "build/android_buildflags.h"
#include "chrome/browser/android/tab_android.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/tab_contents/tab_util.h"
#include "chrome/browser/ui/android/tab_model/tab_model_list.h"
#include "chrome/browser/ui/android/tab_model/tab_model_observer_jni_bridge.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/ui/tabs/tab_enums.h"
#include "chrome/browser/ui/tabs/tab_muted_utils.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"
#include "content/public/common/resource_request_body_android.h"
#include "content/public/common/url_constants.h"
#include "ui/base/unowned_user_data/scoped_unowned_user_data.h"
#include "ui/base/window_open_disposition.h"
#include "url/android/gurl_android.h"
#include "url/origin.h"

// "chrome/browser/ui/browser_window" is available on desktop Android, but not
// other Android builds.
#if BUILDFLAG(IS_DESKTOP_ANDROID)
#include "chrome/browser/ui/browser_window/internal/android/android_browser_window.h"  // nogncheck
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"  // nogncheck
#include "chrome/browser/ui/browser_window/public/browser_window_interface_iterator.h"  // nogncheck
#endif

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/android/chrome_jni_headers/TabModelJniBridge_jni.h"

using base::android::AttachCurrentThread;
using base::android::ConvertUTF8ToJavaString;
using base::android::JavaParamRef;
using base::android::SafeGetArrayLength;
using base::android::ScopedJavaLocalRef;
using chrome::android::ActivityType;
using content::WebContents;

namespace {

std::vector<TabAndroid*> GetAllTabsFromHandles(
    const std::set<tabs::TabHandle>& handles) {
  std::vector<TabAndroid*> tabs;
  tabs.reserve(tabs.size());
  for (tabs::TabHandle handle : handles) {
    TabAndroid* tab_android = TabAndroid::FromTabHandle(handle);
    if (!tab_android) {
      continue;
    }
    tabs.push_back(tab_android);
  }
  return tabs;
}

#if BUILDFLAG(IS_DESKTOP_ANDROID)
AndroidBrowserWindow* GetAndroidBrowserWindow(SessionID session_id) {
  for (BrowserWindowInterface* window : GetAllBrowserWindowInterfaces()) {
    if (window->GetSessionID() == session_id) {
      return static_cast<AndroidBrowserWindow*>(window);
    }
  }
  return nullptr;
}
#endif  // BUILDFLAG(IS_DESKTOP_ANDROID)

}  // namespace

TabModelJniBridge::TabModelJniBridge(JNIEnv* env,
                                     const jni_zero::JavaRef<jobject>& jobj,
                                     Profile* profile,
                                     ActivityType activity_type,
                                     bool is_archived_tab_model)
    : TabModel(profile, activity_type),
      java_object_(env, jobj),
      is_archived_tab_model_(is_archived_tab_model) {
  // The archived tab model isn't tracked in native, except to comply with clear
  // browsing data.
  if (is_archived_tab_model_) {
    TabModelList::SetArchivedTabModel(this);
  } else {
    TabModelList::AddTabModel(this);
  }
}

void TabModelJniBridge::Destroy(JNIEnv* env) {
  delete this;
}

void TabModelJniBridge::AssociateWithBrowserWindow(
    JNIEnv* env,
    long native_android_browser_window) {
// BrowserWindowInterface is available on desktop Android, but not other Android
// builds. For non-desktop Android, this function should be a no-op.
#if BUILDFLAG(IS_DESKTOP_ANDROID)
  BrowserWindowInterface* android_browser_window =
      reinterpret_cast<BrowserWindowInterface*>(native_android_browser_window);
  CHECK(android_browser_window != nullptr);

  scoped_unowned_user_data_ =
      std::make_unique<ui::ScopedUnownedUserData<TabModel>>(
          android_browser_window->GetUnownedUserDataHost(), *this);
  SetSessionId(android_browser_window->GetSessionID());
#endif
}

void TabModelJniBridge::TabAddedToModel(JNIEnv* env,
                                        TabAndroid* tab) {
  // Tab#initialize() should have been called by now otherwise we can't push
  // the window id.
  if (tab) {
    tab->SetWindowSessionID(GetSessionId());
  }

  // Count tabs that are used for incognito mode inside the browser (excluding
  // off-the-record tabs for incognito CCTs, etc.).
  if (GetProfile()->IsIncognitoProfile()) {
    UMA_HISTOGRAM_COUNTS_100("Tab.Count.Incognito", GetTabCount());
  }
}

TabAndroid* TabModelJniBridge::DuplicateTab(JNIEnv* env, TabAndroid* tab) {
  return static_cast<TabAndroid*>(DuplicateTab(tab));
}

void TabModelJniBridge::MoveTabToWindowForTesting(
    JNIEnv* env,
    TabAndroid* tab,
    long android_browser_window_ptr,
    int new_index) {
#if BUILDFLAG(IS_DESKTOP_ANDROID)
  SessionID destination_window_id =
      reinterpret_cast<AndroidBrowserWindow*>(android_browser_window_ptr)
          ->GetSessionID();
  MoveTabToWindow(tab->GetHandle(), destination_window_id, new_index);
#else
  NOTIMPLEMENTED();
#endif  // BUILDFLAG(IS_DESKTOP_ANDROID)
}

void TabModelJniBridge::MoveTabGroupToWindowForTesting(
    JNIEnv* env,
    const base::Token& group_id,
    long android_browser_window_ptr,
    int new_index) {
#if BUILDFLAG(IS_DESKTOP_ANDROID)
  SessionID destination_window_id =
      reinterpret_cast<AndroidBrowserWindow*>(android_browser_window_ptr)
          ->GetSessionID();
  MoveTabGroupToWindow(tab_groups::TabGroupId::FromRawToken(group_id),
                       destination_window_id, new_index);
#else
  NOTIMPLEMENTED();
#endif  // BUILDFLAG(IS_DESKTOP_ANDROID)
}

void TabModelJniBridge::SetMuteSetting(JNIEnv* env,
                                       std::vector<TabAndroid*> tabs,
                                       bool mute) {
  Profile* profile = GetProfile();
  HostContentSettingsMap* map =
      HostContentSettingsMapFactory::GetForProfile(profile);
  bool offTheRecord = profile->IsOffTheRecord();
  std::set<url::Origin> processed_origins;

  for (TabAndroid* tab : tabs) {
    WebContents* web_contents = tab->web_contents();

    // If there are no WebContents, we get the url from the Tab object, if
    // available.
    GURL url =
        web_contents ? web_contents->GetLastCommittedURL() : tab->GetURL();

    if (url.is_empty()) {
      continue;
    }

    if (url.SchemeIs(content::kChromeUIScheme) ||
        url.SchemeIs(content::kChromeNativeScheme)) {
      if (web_contents) {
        // chrome:// URLs don't have content settings but can be muted, so just
        // mute the WebContents.
        SetTabAudioMuted(web_contents, mute,
                         TabMutedReason::kContentSettingChrome, std::string());
      }
      continue;
    }

    if (web_contents) {
      // The origin may be null (when navigation hasn't finalized) or may not
      // match the URL (e.g., for offline pages). We use the URL directly in
      // these cases to ensure the content setting is applied correctly, but the
      // origin can help us filter previously processed origins.
      url::Origin origin =
          web_contents->GetPrimaryMainFrame()->GetLastCommittedOrigin();
      if (!processed_origins.insert(origin).second) {
        continue;
      }
    }

    ContentSetting setting =
        mute ? CONTENT_SETTING_BLOCK : CONTENT_SETTING_ALLOW;

    // We add the site URL to the exception list if the request behavior
    // differs from the default value.
    if (!offTheRecord) {
      // Using default setting value below clears the setting from the
      // exception list for the site URL if it exists or if there is an
      // existing less specific rule in the exception list.
      map->SetContentSettingDefaultScope(url, url, ContentSettingsType::SOUND,
                                         CONTENT_SETTING_DEFAULT);

      // If the current setting matches the desired setting after clearing the
      // site URL from the exception list we can simply skip.
      if (setting ==
          map->GetContentSetting(url, url, ContentSettingsType::SOUND)) {
        continue;
      }
    }
    // Adds the site URL to the exception list for the setting.
    map->SetContentSettingDefaultScope(url, url, ContentSettingsType::SOUND,
                                       setting);
  }
}

jint TabModelJniBridge::GetSessionIdForTesting(JNIEnv* env) {
  return GetSessionId().id();
}

ActivityType TabModelJniBridge::GetActivityTypeForTesting(JNIEnv* env) {
  return activity_type();
}

void TabModelJniBridge::AddTabListInterfaceObserver(
    TabListInterfaceObserver* observer) {
  // If a first observer is being added then instantiate an observer bridge.
  if (!observer_bridge_) {
    JNIEnv* env = AttachCurrentThread();
    observer_bridge_ = std::make_unique<TabModelObserverJniBridge>(
        env, java_object_.get(env), *this);
  }
  observer_bridge_->AddTabListInterfaceObserver(observer);
}

void TabModelJniBridge::RemoveTabListInterfaceObserver(
    TabListInterfaceObserver* observer) {
  observer_bridge_->RemoveTabListInterfaceObserver(observer);

  // Tear down the bridge if there are no observers left.
  if (!observer_bridge_->has_observers()) {
    observer_bridge_.reset();
  }
}

int TabModelJniBridge::GetTabCount() const {
  JNIEnv* env = AttachCurrentThread();
  return Java_TabModelJniBridge_getCount(env, java_object_.get(env));
}

int TabModelJniBridge::GetActiveIndex() const {
  JNIEnv* env = AttachCurrentThread();
  return Java_TabModelJniBridge_index(env, java_object_.get(env));
}

tabs::TabInterface* TabModelJniBridge::GetActiveTab() {
  return GetTab(GetActiveIndex());
}

void TabModelJniBridge::CreateTab(TabAndroid* parent,
                                  WebContents* web_contents,
                                  bool select) {
  JNIEnv* env = AttachCurrentThread();
  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());

  Java_TabModelJniBridge_createTabWithWebContents(
      env, java_object_.get(env), (parent ? parent->GetJavaObject() : nullptr),
      profile->GetJavaObject(), web_contents->GetJavaWebContents(), select);
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
  ScopedJavaLocalRef<jobject> jurl = url::GURLAndroid::FromNativeGURL(env, url);
  ScopedJavaLocalRef<jobject> jinitiator_origin =
      params->initiator_origin ? params->initiator_origin->ToJavaObject(env)
                               : nullptr;
  ScopedJavaLocalRef<jobject> jpost_data =
      content::ConvertResourceRequestBodyToJavaObject(env, params->post_data);
  Java_TabModelJniBridge_openNewTab(
      env, jobj, parent->GetJavaObject(), jurl, jinitiator_origin,
      params->extra_headers, jpost_data, static_cast<int>(disposition),
      params->opened_by_another_window, params->is_renderer_initiated);
}

WebContents* TabModelJniBridge::GetWebContentsAt(int index) const {
  TabAndroid* tab = GetTabAt(index);
  return tab == nullptr ? nullptr : tab->web_contents();
}

TabAndroid* TabModelJniBridge::GetTabAt(int index) const {
  JNIEnv* env = AttachCurrentThread();
  return Java_TabModelJniBridge_getTabAt(env, java_object_.get(env), index);
}

ScopedJavaLocalRef<jobject> TabModelJniBridge::GetJavaObject() const {
  JNIEnv* env = AttachCurrentThread();
  return java_object_.get(env);
}

void TabModelJniBridge::SetActiveIndex(int index) {
  JNIEnv* env = AttachCurrentThread();
  Java_TabModelJniBridge_setIndex(env, java_object_.get(env), index);
}

void TabModelJniBridge::ForceCloseAllTabs() {
  JNIEnv* env = AttachCurrentThread();
  Java_TabModelJniBridge_forceCloseAllTabs(env, java_object_.get(env));
}

void TabModelJniBridge::CloseTabAt(int index) {
  JNIEnv* env = AttachCurrentThread();
  Java_TabModelJniBridge_closeTabAt(env, java_object_.get(env), index);
}

WebContents* TabModelJniBridge::CreateNewTabForDevTools(const GURL& url,
                                                        bool new_window) {
  // TODO(dfalcantara): Change the Java side so that it creates and returns the
  //                    WebContents, which we can load the URL on and return.
  JNIEnv* env = AttachCurrentThread();
  TabAndroid* tab = Java_TabModelJniBridge_createNewTabForDevTools(
      env, java_object_.get(env), url::GURLAndroid::FromNativeGURL(env, url),
      new_window);
  if (!tab) {
    VLOG(0) << "Failed to create java tab";
    return nullptr;
  }
  return tab->web_contents();
}

bool TabModelJniBridge::IsSessionRestoreInProgress() const {
  JNIEnv* env = AttachCurrentThread();
  return Java_TabModelJniBridge_isSessionRestoreInProgress(
      env, java_object_.get(env));
}

bool TabModelJniBridge::IsActiveModel() const {
  JNIEnv* env = AttachCurrentThread();
  return Java_TabModelJniBridge_isActiveModel(env, java_object_.get(env));
}

void TabModelJniBridge::AddObserver(TabModelObserver* observer) {
  // If a first observer is being added then instantiate an observer bridge.
  if (!observer_bridge_) {
    JNIEnv* env = AttachCurrentThread();
    observer_bridge_ = std::make_unique<TabModelObserverJniBridge>(
        env, java_object_.get(env), *this);
  }
  observer_bridge_->AddObserver(observer);
}

void TabModelJniBridge::RemoveObserver(TabModelObserver* observer) {
  observer_bridge_->RemoveObserver(observer);

  // Tear down the bridge if there are no observers left.
  if (!observer_bridge_->has_observers()) {
    observer_bridge_.reset();
  }
}

void TabModelJniBridge::BroadcastSessionRestoreComplete(JNIEnv* env) {
  if (!is_archived_tab_model_) {
    TabModel::BroadcastSessionRestoreComplete();
  }
}

int TabModelJniBridge::GetTabCountNavigatedInTimeWindow(
    const base::Time& begin_time,
    const base::Time& end_time) const {
  JNIEnv* env = AttachCurrentThread();
  int64_t begin_time_ms = begin_time.InMillisecondsSinceUnixEpoch();
  int64_t end_time_ms = end_time.InMillisecondsSinceUnixEpoch();
  return Java_TabModelJniBridge_getTabCountNavigatedInTimeWindow(
      env, java_object_.get(env), begin_time_ms, end_time_ms);
}

void TabModelJniBridge::CloseTabsNavigatedInTimeWindow(
    const base::Time& begin_time,
    const base::Time& end_time) {
  JNIEnv* env = AttachCurrentThread();
  int64_t begin_time_ms = begin_time.InMillisecondsSinceUnixEpoch();
  int64_t end_time_ms = end_time.InMillisecondsSinceUnixEpoch();
  return Java_TabModelJniBridge_closeTabsNavigatedInTimeWindow(
      env, java_object_.get(env), begin_time_ms, end_time_ms);
}

tabs::TabInterface* TabModelJniBridge::OpenTab(const GURL& url, int index) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> jobj = java_object_.get(env);
  ScopedJavaLocalRef<jobject> jurl = url::GURLAndroid::FromNativeGURL(env, url);

  return Java_TabModelJniBridge_openTabProgrammatically(env, jobj, jurl, index);
}

void TabModelJniBridge::DiscardTab(tabs::TabHandle tab) {
  if (!base::FeatureList::IsEnabled(features::kWebContentsDiscard)) {
    return;
  }

  TabAndroid* tab_android = TabAndroid::FromTabHandle(tab);
  // For now just don't discard the activated tab. This ruleset could be refined
  // in the future.
  if (!tab_android || tab_android->IsActivated()) {
    return;
  }

  content::WebContents* web_contents = tab_android->web_contents();
  if (!web_contents) {
    return;
  }
  web_contents->Discard(base::DoNothing());
}

tabs::TabInterface* TabModelJniBridge::DuplicateTab(tabs::TabHandle tab) {
  TabAndroid* tab_android = TabAndroid::FromTabHandle(tab);
  return DuplicateTab(tab_android);
}

tabs::TabInterface* TabModelJniBridge::DuplicateTab(TabAndroid* tab) {
  WebContents* web_contents = tab == nullptr ? nullptr : tab->web_contents();
  if (!web_contents) {
    return nullptr;
  }

  std::unique_ptr<WebContents> cloned_web_contents = web_contents->Clone();
  ScopedJavaLocalRef<jobject> jweb_contents =
      cloned_web_contents->GetJavaWebContents();
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> jobj = java_object_.get(env);

  TabAndroid* new_tab =
      Java_TabModelJniBridge_duplicateTab(env, jobj, tab, jweb_contents);
  cloned_web_contents.release();

  return new_tab;
}

tabs::TabInterface* TabModelJniBridge::GetTab(int index) {
  return GetTabAt(index);
}

int TabModelJniBridge::GetIndexOfTab(tabs::TabHandle tab) {
  tabs::TabInterface* tab_interface = tab.Get();
  if (!tab_interface) {
    return -1;
  }
  int count = GetTabCount();
  for (int i = 0; i < count; ++i) {
    if (GetTabAt(i) == tab_interface) {
      return i;
    }
  }

  return -1;
}

void TabModelJniBridge::HighlightTabs(tabs::TabHandle tab_to_activate,
                                      const std::set<tabs::TabHandle>& tabs) {
  std::vector<TabAndroid*> tabs_to_highlight = GetAllTabsFromHandles(tabs);
  TabAndroid* tab_android = TabAndroid::FromTabHandle(tab_to_activate);
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> jobj = java_object_.get(env);
  Java_TabModelJniBridge_highlightTabs(env, jobj, tab_android,
                                       tabs_to_highlight);
}

void TabModelJniBridge::MoveTab(tabs::TabHandle tab, int index) {
  TabAndroid* tab_android = TabAndroid::FromTabHandle(tab);
  if (!tab_android) {
    return;
  }

  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> jobj = java_object_.get(env);
  Java_TabModelJniBridge_moveTabToIndex(env, jobj, tab_android, index);
}

void TabModelJniBridge::CloseTab(tabs::TabHandle tab) {
  TabAndroid* tab_android = TabAndroid::FromTabHandle(tab);
  if (!tab_android) {
    return;
  }

  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> jobj = java_object_.get(env);
  Java_TabModelJniBridge_closeTab(env, jobj, tab_android);
}

std::vector<tabs::TabInterface*> TabModelJniBridge::GetAllTabs() {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> jobj = java_object_.get(env);
  std::vector<TabAndroid*> tab_androids =
      Java_TabModelJniBridge_getAllTabs(env, jobj);

  std::vector<tabs::TabInterface*> tabs;
  tabs.reserve(tab_androids.size());
  for (TabAndroid* tab_android : tab_androids) {
    tabs.push_back(static_cast<tabs::TabInterface*>(tab_android));
  }
  return tabs;
}

void TabModelJniBridge::PinTab(tabs::TabHandle tab) {
  TabAndroid* tab_android = TabAndroid::FromTabHandle(tab);
  if (!tab_android) {
    return;
  }

  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> jobj = java_object_.get(env);
  Java_TabModelJniBridge_pinTab(env, jobj, tab_android);
}

void TabModelJniBridge::UnpinTab(tabs::TabHandle tab) {
  TabAndroid* tab_android = TabAndroid::FromTabHandle(tab);
  if (!tab_android) {
    return;
  }

  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> jobj = java_object_.get(env);
  Java_TabModelJniBridge_unpinTab(env, jobj, tab_android);
}

std::optional<tab_groups::TabGroupId> TabModelJniBridge::AddTabsToGroup(
    std::optional<tab_groups::TabGroupId> group_id,
    const std::set<tabs::TabHandle>& tabs) {
  std::optional<base::Token> requested_group_id =
      tab_groups::TabGroupId::ToOptionalToken(group_id);
  std::vector<TabAndroid*> tabs_to_add = GetAllTabsFromHandles(tabs);

  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> jobj = java_object_.get(env);
  std::optional<base::Token> final_group_id =
      Java_TabModelJniBridge_addTabsToGroup(env, jobj, requested_group_id,
                                            tabs_to_add);
  return tab_groups::TabGroupId::FromOptionalToken(final_group_id);
}

void TabModelJniBridge::Ungroup(const std::set<tabs::TabHandle>& tabs) {
  std::vector<TabAndroid*> tabs_to_ungroup = GetAllTabsFromHandles(tabs);
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> jobj = java_object_.get(env);
  Java_TabModelJniBridge_ungroup(env, jobj, tabs_to_ungroup);
}

void TabModelJniBridge::MoveGroupTo(tab_groups::TabGroupId group_id,
                                    int index) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> jobj = java_object_.get(env);
  Java_TabModelJniBridge_moveGroupToIndex(env, jobj, group_id.token(), index);
}

void TabModelJniBridge::MoveTabToWindow(tabs::TabHandle tab,
                                        SessionID destination_window_id,
                                        int destination_index) {
  TabAndroid* tab_android = TabAndroid::FromTabHandle(tab);
  if (!tab_android) {
    return;
  }

  ScopedJavaLocalRef<jobject> jactivity =
      GetActivityForWindow(destination_window_id);
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> jobj = java_object_.get(env);
  Java_TabModelJniBridge_moveTabToWindowInternal(env, jobj, tab_android,
                                                 jactivity, destination_index);
}

void TabModelJniBridge::MoveTabGroupToWindow(tab_groups::TabGroupId group_id,
                                             SessionID destination_window_id,
                                             int destination_index) {
  ScopedJavaLocalRef<jobject> jactivity =
      GetActivityForWindow(destination_window_id);
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> jobj = java_object_.get(env);
  Java_TabModelJniBridge_moveTabGroupToWindowInternal(
      env, jobj, group_id.token(), jactivity, destination_index);
}

ScopedJavaLocalRef<jobject> TabModelJniBridge::GetActivityForWindow(
    SessionID window_id) {
  ScopedJavaLocalRef<jobject> jactivity;
#if BUILDFLAG(IS_DESKTOP_ANDROID)
  AndroidBrowserWindow* window = GetAndroidBrowserWindow(window_id);
  if (!window) {
    return jactivity;
  }
  CHECK_EQ(window->GetProfile()->IsOffTheRecord(),
           GetProfile()->IsOffTheRecord());
  jactivity = window->GetActivity();
#endif  // BUILDFLAG(IS_DESKTOP_ANDROID)
  return jactivity;
}

// static
jclass TabModelJniBridge::GetClazz(JNIEnv* env) {
  return org_chromium_chrome_browser_tabmodel_TabModelJniBridge_clazz(env);
}

TabModelJniBridge::~TabModelJniBridge() {
  if (is_archived_tab_model_) {
    TabModelList::SetArchivedTabModel(nullptr);
  } else {
    TabModelList::RemoveTabModel(this);
  }
}

static jlong JNI_TabModelJniBridge_Init(JNIEnv* env,
                                        const JavaParamRef<jobject>& obj,
                                        Profile* profile,
                                        jint j_activity_type,
                                        unsigned char is_archived_tab_model) {
  TabModel* tab_model = new TabModelJniBridge(
      env, obj, profile, static_cast<ActivityType>(j_activity_type),
      is_archived_tab_model);
  return reinterpret_cast<intptr_t>(tab_model);
}

DEFINE_JNI(TabModelJniBridge)
