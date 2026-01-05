// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sessions/content/content_live_tab.h"

#include <memory>

#include "base/memory/ptr_util.h"
#include "components/sessions/content/content_platform_specific_tab_data.h"
#include "content/public/browser/navigation_controller.h"
#include "third_party/blink/public/common/user_agent/user_agent_metadata.h"

namespace sessions {

ContentLiveTab::ContentLiveTab(content::WebContents* contents)
    : content::WebContentsUserData<ContentLiveTab>(*contents) {}

ContentLiveTab::~ContentLiveTab() = default;

bool ContentLiveTab::IsInitialBlankNavigation() {
  return navigation_controller().IsInitialBlankNavigation();
}

int ContentLiveTab::GetCurrentEntryIndex() {
  return navigation_controller().GetCurrentEntryIndex();
}

int ContentLiveTab::GetPendingEntryIndex() {
  return navigation_controller().GetPendingEntryIndex();
}

sessions::SerializedNavigationEntry ContentLiveTab::GetEntryAtIndex(int index) {
  return sessions::ContentSerializedNavigationBuilder::FromNavigationEntry(
      index, navigation_controller().GetEntryAtIndex(index));
}

sessions::SerializedNavigationEntry ContentLiveTab::GetPendingEntry() {
  return sessions::ContentSerializedNavigationBuilder::FromNavigationEntry(
      GetPendingEntryIndex(), navigation_controller().GetPendingEntry());
}

int ContentLiveTab::GetEntryCount() {
  return navigation_controller().GetEntryCount();
}

std::unique_ptr<tab_restore::PlatformSpecificTabData>
ContentLiveTab::GetPlatformSpecificTabData() {
  return std::make_unique<sessions::ContentPlatformSpecificTabData>(
      &GetWebContents());
}

SerializedUserAgentOverride ContentLiveTab::GetUserAgentOverride() {
  const blink::UserAgentOverride& ua_override =
      GetWebContents().GetUserAgentOverride();
  SerializedUserAgentOverride serialized_ua_override;
  serialized_ua_override.ua_string_override = ua_override.ua_string_override;
  serialized_ua_override.opaque_ua_metadata_override =
      blink::UserAgentMetadata::Marshal(ua_override.ua_metadata_override);
  return serialized_ua_override;
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(ContentLiveTab);

}  // namespace sessions
