// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CONTENT_SETTINGS_BROWSER_TEST_PAGE_SPECIFIC_CONTENT_SETTINGS_DELEGATE_H_
#define COMPONENTS_CONTENT_SETTINGS_BROWSER_TEST_PAGE_SPECIFIC_CONTENT_SETTINGS_DELEGATE_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "components/content_settings/browser/page_specific_content_settings.h"

namespace content_settings {

class TestPageSpecificContentSettingsDelegate
    : public PageSpecificContentSettings::Delegate {
 public:
  TestPageSpecificContentSettingsDelegate(PrefService* prefs,
                                          HostContentSettingsMap* settings_map);
  ~TestPageSpecificContentSettingsDelegate() override;

  // PageSpecificContentSettings::Delegate:
  void UpdateLocationBar() override;
  PrefService* GetPrefs() override;
  HostContentSettingsMap* GetSettingsMap() override;
  std::unique_ptr<BrowsingDataModel::Delegate> CreateBrowsingDataModelDelegate()
      override;
  void SetDefaultRendererContentSettingRules(
      content::RenderFrameHost* rfh,
      RendererContentSettingRules* rules) override;
  std::vector<storage::FileSystemType> GetAdditionalFileSystemTypes() override;
  browsing_data::CookieHelper::IsDeletionDisabledCallback
  GetIsDeletionDisabledCallback() override;
  bool IsMicrophoneCameraStateChanged(
      PageSpecificContentSettings::MicrophoneCameraState
          microphone_camera_state,
      const std::string& media_stream_selected_audio_device,
      const std::string& media_stream_selected_video_device) override;
  PageSpecificContentSettings::MicrophoneCameraState GetMicrophoneCameraState()
      override;
  content::WebContents* MaybeGetSyncedWebContentsForPictureInPicture(
      content::WebContents* web_contents) override;
  void OnContentAllowed(ContentSettingsType type) override;
  void OnContentBlocked(ContentSettingsType type) override;
  void OnStorageAccessAllowed(
      content_settings::mojom::ContentSettingsManager::StorageType storage_type,
      const url::Origin& origin,
      content::Page& page) override;
  void OnCookieAccessAllowed(const net::CookieList& accessed_cookies,
                             content::Page& page) override;
  void OnServiceWorkerAccessAllowed(const url::Origin& origin,
                                    content::Page& page) override;

 private:
  raw_ptr<PrefService> prefs_;
  scoped_refptr<HostContentSettingsMap> settings_map_;
};

}  // namespace content_settings

#endif  // COMPONENTS_CONTENT_SETTINGS_BROWSER_TEST_PAGE_SPECIFIC_CONTENT_SETTINGS_DELEGATE_H_
