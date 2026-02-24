// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/tab_model/tab_model_jni_bridge.h"

#include <jni.h>
#include <stdint.h>

#include <cstdint>
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
#include "chrome/browser/flags/android/chrome_feature_list.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/tab_contents/tab_util.h"
#include "chrome/browser/ui/android/tab_model/tab_model.h"
#include "chrome/browser/ui/android/tab_model/tab_model_list.h"
#include "chrome/browser/ui/android/tab_model/tab_model_observer_jni_bridge.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/ui/browser_window/internal/android/android_browser_window.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface_iterator.h"
#include "chrome/browser/ui/tabs/tab_enums.h"
#include "chrome/browser/ui/tabs/tab_muted_utils.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/tab_groups/tab_group_id.h"
#include "components/tab_groups/tab_group_visual_data.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"
#include "content/public/common/resource_request_body_android.h"
#include "content/public/common/url_constants.h"
#include "third_party/jni_zero/jni_zero.h"
#include "ui/base/unowned_user_data/scoped_unowned_user_data.h"
#include "ui/base/window_open_disposition.h"
#include "ui/gfx/range/range.h"
#include "url/android/gurl_android.h"
#include "url/origin.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/android/chrome_jni_headers/TabModelJniBridge_jni.h"

using base::android::AttachCurrentThread;
using base::android::ConvertUTF8ToJavaString;
using base::android::JavaRef;
using base::android::SafeGetArrayLength;
using base::android::ScopedJavaLocalRef;
using chrome::android::ActivityType;
using content::WebContents;
using tab_groups::TabGroupColorId;
using tab_groups::TabGroupVisualData;

namespace {

// Represents INVALID_COLOR_ID from TabGroupColorUtils.java.
constexpr int kInvalidTabGroupColorId = -1;

// Returns a vector of TabAndroid* from a container (e.g. a std::set or a
// std::vector) of TabHandle.
template <typename Container>
std::vector<TabAndroid*> GetAllTabsFromHandles(const Container& handles) {
  std::vector<TabAndroid*> tabs;
  tabs.reserve(handles.size());
  for (tabs::TabHandle handle : handles) {
    if (TabAndroid* tab_android = TabAndroid::FromTabHandle(handle)) {
      tabs.push_back(tab_android);
    }
  }
  return tabs;
}

AndroidBrowserWindow* GetAndroidBrowserWindow(SessionID session_id) {
  for (BrowserWindowInterface* window : GetAllBrowserWindowInterfaces()) {
    if (window->GetSessionID() == session_id) {
      return static_cast<AndroidBrowserWindow*>(window);
    }
  }
  return nullptr;
}

}  // namespace

TabModelJniBridge::TabModelJniBridge(JNIEnv* env,
                                     const jni_zero::JavaRef<jobject>& jobj,
                                     Profile* profile,
                                     ActivityType activity_type,
                                     TabModelType tab_model_type)
    : TabModel(profile, activity_type, tab_model_type),
      java_object_(env, jobj) {
  // The archived tab model isn't tracked in native, except to comply with clear
  // browsing data.
  if (GetTabModelType() == TabModelType::kArchived) {
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
  if (!TabModel::EnableBrowserWindowInterfaceMobile()) {
    return;
  }
  BrowserWindowInterface* android_browser_window =
      reinterpret_cast<BrowserWindowInterface*>(native_android_browser_window);
  CHECK(android_browser_window != nullptr);

  scoped_unowned_user_data_ =
      std::make_unique<ui::ScopedUnownedUserData<TabListInterface>>(
          android_browser_window->GetUnownedUserDataHost(), *this);
  SetSessionId(android_browser_window->GetSessionID());
}

void TabModelJniBridge::DissociateWithBrowserWindow(JNIEnv* env) {
  if (!TabModel::EnableBrowserWindowInterfaceMobile()) {
    return;
  }
  CHECK(scoped_unowned_user_data_ != nullptr);
  scoped_unowned_user_data_.reset();
  SetSessionId(SessionID::InvalidValue());
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
  if (!TabModel::EnableBrowserWindowInterfaceMobile()) {
    return;
  }
  SessionID destination_window_id =
      reinterpret_cast<AndroidBrowserWindow*>(android_browser_window_ptr)
          ->GetSessionID();
  MoveTabToWindow(tab->GetHandle(), destination_window_id, new_index);
}

void TabModelJniBridge::MoveTabGroupToWindowForTesting(
    JNIEnv* env,
    const base::Token& group_id,
    long android_browser_window_ptr,
    int new_index) {
  if (!TabModel::EnableBrowserWindowInterfaceMobile()) {
    return;
  }
  SessionID destination_window_id =
      reinterpret_cast<AndroidBrowserWindow*>(android_browser_window_ptr)
          ->GetSessionID();
  MoveTabGroupToWindow(tab_groups::TabGroupId::FromRawToken(group_id),
                       destination_window_id, new_index);
}

bool TabModelJniBridge::IsThisTabListEditable() {
  std::vector<tabs::TabInterface*> all_tabs = GetAllTabs();
  for (auto* tab : all_tabs) {
    if (static_cast<TabAndroid*>(tab)->IsDragging()) {
      return false;
    }
  }

  return true;
}

bool TabModelJniBridge::IsClosingAllTabs() {
  JNIEnv* env = AttachCurrentThread();
  return Java_TabModelJniBridge_isClosingAllTabs(env, java_object_.get(env));
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

int32_t TabModelJniBridge::GetSessionIdForTesting(JNIEnv* env) {
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

tabs::TabInterface* TabModelJniBridge::CreateTab(
    TabAndroid* parent,
    std::unique_ptr<WebContents> web_contents,
    int index,
    TabLaunchType type,
    bool should_pin) {
  JNIEnv* env = AttachCurrentThread();
  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());

  TabAndroid* new_tab = Java_TabModelJniBridge_createTabWithWebContents(
      env, java_object_.get(env), (parent ? parent->GetJavaObject() : nullptr),
      profile->GetJavaObject(), web_contents->GetJavaWebContents(), index,
      static_cast<int>(type), should_pin);
  // If new tab creation is successful, Java assumes ownership of the lifetime
  // of the cloned WebContents.
  if (new_tab) {
    web_contents.release();
  }

  return new_tab;
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
      params->opened_by_another_window, params->is_renderer_initiated,
      params->user_gesture);
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
  if (GetTabModelType() != TabModelType::kArchived) {
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

void TabModelJniBridge::ActivateTab(tabs::TabHandle tab) {
  int index = GetIndexOfTab(tab);
  CHECK_NE(-1, index);
  SetActiveIndex(index);
}

tabs::TabInterface* TabModelJniBridge::OpenTab(const GURL& url, int index) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> jobj = java_object_.get(env);
  ScopedJavaLocalRef<jobject> jurl = url::GURLAndroid::FromNativeGURL(env, url);

  return Java_TabModelJniBridge_openTabProgrammatically(env, jobj, jurl, index);
}

void TabModelJniBridge::SetOpenerForTab(tabs::TabHandle target,
                                        tabs::TabHandle opener) {
  TabAndroid* target_tab = TabAndroid::FromTabHandle(target);
  TabAndroid* opener_tab = TabAndroid::FromTabHandle(opener);
  if (!target_tab || !opener_tab) {
    return;
  }
  JNIEnv* env = AttachCurrentThread();
  Java_TabModelJniBridge_setOpenerForTab(env, target_tab, opener_tab);
}

tabs::TabInterface* TabModelJniBridge::GetOpenerForTab(tabs::TabHandle target) {
  TabAndroid* target_tab = TabAndroid::FromTabHandle(target);
  if (!target_tab) {
    return nullptr;
  }
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> jobj = java_object_.get(env);
  return Java_TabModelJniBridge_getOpenerForTab(env, jobj, target_tab);
}

tabs::TabInterface* TabModelJniBridge::InsertWebContentsAt(
    int index,
    std::unique_ptr<content::WebContents> web_contents,
    bool should_pin,
    std::optional<tab_groups::TabGroupId> group) {
  JNIEnv* env = AttachCurrentThread();

  TabAndroid* new_tab = Java_TabModelJniBridge_insertWebContentsAt(
      env, java_object_.get(env), index, web_contents->GetJavaWebContents(),
      should_pin, tab_groups::TabGroupId::ToOptionalToken(group));

  // If new tab creation is successful, Java assumes ownership of the lifetime
  // of the WebContents.
  if (new_tab) {
    web_contents.release();
  }

  return new_tab;
}

content::WebContents* TabModelJniBridge::DiscardTab(tabs::TabHandle tab) {
  if (!base::FeatureList::IsEnabled(features::kWebContentsDiscard)) {
    return nullptr;
  }

  TabAndroid* tab_android = TabAndroid::FromTabHandle(tab);
  // For now just don't discard the activated tab. This ruleset could be refined
  // in the future.
  if (!tab_android || tab_android->IsActivated()) {
    return nullptr;
  }

  content::WebContents* web_contents = tab_android->web_contents();
  // Don't discard if there are no WebContents or if the WebContents is already
  // discarded.
  if (!web_contents || web_contents->WasDiscarded()) {
    return nullptr;
  }
  web_contents->Discard(base::DoNothing());
  return web_contents;
}

tabs::TabInterface* TabModelJniBridge::DuplicateTab(tabs::TabHandle tab) {
  TabAndroid* tab_android = TabAndroid::FromTabHandle(tab);
  return DuplicateTab(tab_android);
}

tabs::TabInterface* TabModelJniBridge::DuplicateTab(TabAndroid* tab) {
  WebContents* web_contents = tab ? tab->web_contents() : nullptr;
  if (!web_contents) {
    return nullptr;
  }

  std::unique_ptr<WebContents> cloned_web_contents = web_contents->Clone();
  return CreateTab(tab, std::move(cloned_web_contents), /* index= */ -1,
                   TabLaunchType::FROM_TAB_LIST_INTERFACE, tab->IsPinned());
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

bool TabModelJniBridge::ContainsTabGroup(tab_groups::TabGroupId group_id) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> jobj = java_object_.get(env);
  return Java_TabModelJniBridge_containsTabGroup(env, jobj, group_id.token());
}

std::vector<tab_groups::TabGroupId> TabModelJniBridge::ListTabGroups() {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> jobj = java_object_.get(env);
  std::vector<base::Token> group_id_tokens =
      Java_TabModelJniBridge_listTabGroups(env, jobj);

  // NOTE: Order is not guaranteed by the underlying API, but TabListInterface
  // requires returning a <vector> and not a <set>.
  std::vector<tab_groups::TabGroupId> group_ids;
  group_ids.reserve(group_id_tokens.size());
  for (base::Token token : group_id_tokens) {
    group_ids.push_back(tab_groups::TabGroupId::FromRawToken(token));
  }
  return group_ids;
}

std::optional<TabGroupVisualData> TabModelJniBridge::GetTabGroupVisualData(
    tab_groups::TabGroupId group_id) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> jobj = java_object_.get(env);

  const std::u16string title =
      Java_TabModelJniBridge_getTabGroupTitle(env, jobj, group_id.token());
  const int color =
      Java_TabModelJniBridge_getTabGroupColor(env, jobj, group_id.token());

  if (title.empty() && color == kInvalidTabGroupColorId) {
    return std::nullopt;
  }

  const bool collapsed =
      Java_TabModelJniBridge_getTabGroupCollapsed(env, jobj, group_id.token());

  const TabGroupColorId color_id = (color == kInvalidTabGroupColorId)
                                       ? tab_groups::TabGroupColorId::kGrey
                                       : static_cast<TabGroupColorId>(color);

  return TabGroupVisualData(title, color_id, collapsed);
}

gfx::Range TabModelJniBridge::GetTabGroupTabIndices(
    tab_groups::TabGroupId group_id) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> jobj = java_object_.get(env);
  std::vector<int> range =
      Java_TabModelJniBridge_getTabGroupTabIndices(env, jobj, group_id.token());
  if (range.empty()) {
    return {};
  }

  // The vector is used to hold a range, since our JNI doesn't have a way to
  // return a pair<> or Range directly.
  CHECK_EQ(range.size(), 2u);
  return gfx::Range(range[0], range[1]);
}

std::optional<tab_groups::TabGroupId> TabModelJniBridge::CreateTabGroup(
    const std::vector<tabs::TabHandle>& tabs) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> jobj = java_object_.get(env);
  std::vector<TabAndroid*> tabs_to_add = GetAllTabsFromHandles(tabs);
  std::optional<base::Token> group_id_token =
      Java_TabModelJniBridge_createTabGroup(env, jobj, tabs_to_add);
  return tab_groups::TabGroupId::FromOptionalToken(group_id_token);
}

void TabModelJniBridge::SetTabGroupVisualData(
    tab_groups::TabGroupId group_id,
    const tab_groups::TabGroupVisualData& visual_data) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> jobj = java_object_.get(env);

  // The color cast is safe because the enum values are synced across C++ and
  // Java.
  Java_TabModelJniBridge_setTabGroupVisualData(
      env, jobj, group_id.token(), visual_data.title(),
      std::to_underlying(visual_data.color()), visual_data.is_collapsed(),
      /*animate=*/false);
}

std::optional<tab_groups::TabGroupId> TabModelJniBridge::AddTabsToGroup(
    std::optional<tab_groups::TabGroupId> group_id,
    const std::set<tabs::TabHandle>& tabs) {
  std::optional<base::Token> requested_group_id =
      tab_groups::TabGroupId::ToOptionalToken(group_id);

  // Order the tabs by index to ensure consistency with desktop.
  std::vector<TabAndroid*> tabs_to_add;
  tabs_to_add.reserve(tabs.size());
  for (tabs::TabInterface* tab : GetAllTabs()) {
    if (tabs.contains(tab->GetHandle())) {
      tabs_to_add.push_back(static_cast<TabAndroid*>(tab));
    }
  }

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
  std::vector<int> range =
      Java_TabModelJniBridge_getTabGroupTabIndices(env, jobj, group_id.token());
  if (range.empty()) {
    LOG(ERROR) << "No tab group found to move.";
    return;
  }
  CHECK_EQ(range.size(), 2u);

  // range[1] is actually endIndex+1, so no final + 1 is required.
  int tab_group_width = range[1] - range[0];

  // Android assumes `index` includes the tab group. Win/Mac/Linux desktop
  // assumes `index` is with the tab group removed. For compatibility with
  // desktop, adjust the `index` past the tab group if it is at or to the right
  // of the group's leftmost index.
  if (index >= range[0]) {
    index += tab_group_width - 1;
  }
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
  if (!TabModel::EnableBrowserWindowInterfaceMobile()) {
    return ScopedJavaLocalRef<jobject>();
  }
  AndroidBrowserWindow* window = GetAndroidBrowserWindow(window_id);
  if (!window) {
    return ScopedJavaLocalRef<jobject>();
  }
  CHECK_EQ(window->GetProfile()->IsOffTheRecord(),
           GetProfile()->IsOffTheRecord());
  return window->GetActivity();
}

// static
jclass TabModelJniBridge::GetClazz(JNIEnv* env) {
  return org_chromium_chrome_browser_tabmodel_TabModelJniBridge_clazz(env);
}

// static
bool TabModelJniBridge::IsTabLaunchedInForeground(
    TabLaunchType type,
    bool is_new_tab_incognito,
    bool is_current_model_incognito) {
  JNIEnv* env = AttachCurrentThread();
  return Java_TabModelJniBridge_isTabLaunchedInForeground(
      env, static_cast<int>(type), is_new_tab_incognito,
      is_current_model_incognito);
}

TabModelJniBridge::~TabModelJniBridge() {
  // We need to explicitly do this here (instead of e.g. in the
  // TabModelObserverJniBridge dtor) because otherwise, callers might call back
  // into a partially-destructed TabModel.
  if (observer_bridge_) {
    observer_bridge_->NotifyShutdown();
  }

  if (GetTabModelType() == TabModelType::kArchived) {
    TabModelList::SetArchivedTabModel(nullptr);
  } else {
    TabModelList::RemoveTabModel(this);
  }
}

static int64_t JNI_TabModelJniBridge_Init(JNIEnv* env,
                                          const JavaRef<jobject>& obj,
                                          Profile* profile,
                                          int32_t j_activity_type,
                                          int32_t j_tab_model_type) {
  TabModel* tab_model = new TabModelJniBridge(
      env, obj, profile, static_cast<ActivityType>(j_activity_type),
      static_cast<TabModel::TabModelType>(j_tab_model_type));
  return reinterpret_cast<intptr_t>(tab_model);
}

DEFINE_JNI(TabModelJniBridge)
