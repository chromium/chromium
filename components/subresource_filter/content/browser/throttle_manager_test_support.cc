// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/subresource_filter/content/browser/throttle_manager_test_support.h"

#include "components/content_settings/browser/page_specific_content_settings.h"
#include "components/content_settings/browser/test_page_specific_content_settings_delegate.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/infobars/content/content_infobar_manager.h"
#include "components/safe_browsing/core/browser/db/database_manager.h"
#include "components/subresource_filter/content/browser/subresource_filter_content_settings_manager.h"
#include "components/subresource_filter/content/browser/subresource_filter_profile_context.h"

namespace subresource_filter {

ThrottleManagerTestSupport::ThrottleManagerTestSupport(
    content::WebContents* web_contents) {
  // Set up the state that's required by ProfileInteractionManager.
  HostContentSettingsMap::RegisterProfilePrefs(prefs_.registry());
  content_settings::CookieSettings::RegisterProfilePrefs(prefs_.registry());
  settings_map_ = base::MakeRefCounted<HostContentSettingsMap>(
      &prefs_, /*is_off_the_record=*/false, /*store_last_modified=*/false,
      /*restore_session=*/false, /*should_record_metrics=*/false);
  cookie_settings_ = base::MakeRefCounted<content_settings::CookieSettings>(
      settings_map_.get(), &prefs_,
      /*tracking_protection_settings=*/nullptr,
      /*is_incognito=*/false,
      content_settings::CookieSettings::NoFedCmSharingPermissionsCallback(),
      /*tpcd_metadata_manager=*/nullptr, "");
  profile_context_ = std::make_unique<SubresourceFilterProfileContext>(
      settings_map_.get(), cookie_settings_.get());

  // ProfileInteractionManager assumes that this object is present in the
  // context of the passed-in WebContents.
  content_settings::PageSpecificContentSettings::CreateForWebContents(
      web_contents,
      std::make_unique<
          content_settings::TestPageSpecificContentSettingsDelegate>(
          /*prefs=*/nullptr, settings_map_.get()));
  infobars::ContentInfoBarManager::CreateForWebContents(web_contents);
}

ThrottleManagerTestSupport::~ThrottleManagerTestSupport() {
  settings_map_->ShutdownOnUIThread();
}

void ThrottleManagerTestSupport::SetShouldUseSmartUI(bool enabled) {
  profile_context_->settings_manager()->set_should_use_smart_ui_for_testing(
      enabled);
}

}  // namespace subresource_filter
