// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/content_settings/browser/test_page_specific_content_settings_delegate.h"
#include "base/functional/callback_helpers.h"

namespace content_settings {

TestPageSpecificContentSettingsDelegate::
    TestPageSpecificContentSettingsDelegate(
        PrefService* prefs,
        HostContentSettingsMap* settings_map)
    : prefs_(prefs), settings_map_(settings_map) {}

TestPageSpecificContentSettingsDelegate::
    ~TestPageSpecificContentSettingsDelegate() = default;

void TestPageSpecificContentSettingsDelegate::UpdateLocationBar() {}

PrefService* TestPageSpecificContentSettingsDelegate::GetPrefs() {
  return prefs_;
}

HostContentSettingsMap*
TestPageSpecificContentSettingsDelegate::GetSettingsMap() {
  return settings_map_.get();
}

std::unique_ptr<BrowsingDataModel::Delegate>
TestPageSpecificContentSettingsDelegate::CreateBrowsingDataModelDelegate() {
  return nullptr;
}

void TestPageSpecificContentSettingsDelegate::
    SetDefaultRendererContentSettingRules(content::RenderFrameHost* rfh,
                                          RendererContentSettingRules* rules) {}

PageSpecificContentSettings::MicrophoneCameraState
TestPageSpecificContentSettingsDelegate::GetMicrophoneCameraState() {
  return {};
}

content::WebContents* TestPageSpecificContentSettingsDelegate::
    MaybeGetSyncedWebContentsForPictureInPicture(
        content::WebContents* web_contents) {
  return nullptr;
}

void TestPageSpecificContentSettingsDelegate::OnContentAllowed(
    ContentSettingsType type) {}

void TestPageSpecificContentSettingsDelegate::OnContentBlocked(
    ContentSettingsType type) {}

bool TestPageSpecificContentSettingsDelegate::IsBlockedOnSystemLevel(
    ContentSettingsType type) {
  return false;
}

bool TestPageSpecificContentSettingsDelegate::IsFrameAllowlistedForJavaScript(
    content::RenderFrameHost* render_frame_host) {
  return false;
}

}  // namespace content_settings
