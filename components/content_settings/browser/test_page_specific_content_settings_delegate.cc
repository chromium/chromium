// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/content_settings/browser/test_page_specific_content_settings_delegate.h"

namespace content_settings {

TestPageSpecificContentSettingsDelegate::
    TestPageSpecificContentSettingsDelegate(
        PrefService* prefs,
        HostContentSettingsMap* settings_map)
    : prefs_(prefs), settings_map_(settings_map) {}

TestPageSpecificContentSettingsDelegate::
    ~TestPageSpecificContentSettingsDelegate() = default;

void TestPageSpecificContentSettingsDelegate::UpdateLocationBar() {}

void TestPageSpecificContentSettingsDelegate::SetContentSettingRules(
    content::RenderProcessHost* process,
    const RendererContentSettingRules& rules) {}

PrefService* TestPageSpecificContentSettingsDelegate::GetPrefs() {
  return prefs_;
}

HostContentSettingsMap*
TestPageSpecificContentSettingsDelegate::GetSettingsMap() {
  return settings_map_.get();
}

ContentSetting TestPageSpecificContentSettingsDelegate::GetEmbargoSetting(
    const GURL& request_origin,
    ContentSettingsType permission) {
  return ContentSetting::CONTENT_SETTING_ASK;
}

std::vector<storage::FileSystemType>
TestPageSpecificContentSettingsDelegate::GetAdditionalFileSystemTypes() {
  return {};
}

browsing_data::CookieHelper::IsDeletionDisabledCallback
TestPageSpecificContentSettingsDelegate::GetIsDeletionDisabledCallback() {
  return base::NullCallback();
}

bool TestPageSpecificContentSettingsDelegate::IsMicrophoneCameraStateChanged(
    PageSpecificContentSettings::MicrophoneCameraState microphone_camera_state,
    const std::string& media_stream_selected_audio_device,
    const std::string& media_stream_selected_video_device) {
  return false;
}

PageSpecificContentSettings::MicrophoneCameraState
TestPageSpecificContentSettingsDelegate::GetMicrophoneCameraState() {
  return PageSpecificContentSettings::MICROPHONE_CAMERA_NOT_ACCESSED;
}

void TestPageSpecificContentSettingsDelegate::OnContentAllowed(
    ContentSettingsType type) {}

void TestPageSpecificContentSettingsDelegate::OnContentBlocked(
    ContentSettingsType type) {}

void TestPageSpecificContentSettingsDelegate::OnCacheStorageAccessAllowed(
    const url::Origin& origin) {}

void TestPageSpecificContentSettingsDelegate::OnCookieAccessAllowed(
    const net::CookieList& accessed_cookies) {}

void TestPageSpecificContentSettingsDelegate::OnDomStorageAccessAllowed(
    const url::Origin& origin) {}

void TestPageSpecificContentSettingsDelegate::OnFileSystemAccessAllowed(
    const url::Origin& origin) {}

void TestPageSpecificContentSettingsDelegate::OnIndexedDBAccessAllowed(
    const url::Origin& origin) {}

void TestPageSpecificContentSettingsDelegate::OnServiceWorkerAccessAllowed(
    const url::Origin& origin) {}

void TestPageSpecificContentSettingsDelegate::OnWebDatabaseAccessAllowed(
    const url::Origin& origin) {}

}  // namespace content_settings
