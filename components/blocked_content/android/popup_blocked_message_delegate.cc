// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/blocked_content/android/popup_blocked_message_delegate.h"

#include "components/blocked_content/android/popup_blocked_helper.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/messages/android/message_dispatcher_bridge.h"
#include "components/prefs/pref_service.h"
#include "components/resources/android/theme_resources.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"

namespace blocked_content {

bool PopupBlockedMessageDelegate::ShowMessage(
    int num_popups,
    HostContentSettingsMap* settings_map,
    base::OnceClosure on_show_popups_callback) {
  if (message_ != nullptr) {  // update title only
    message_->SetTitle(l10n_util::GetPluralStringFUTF16(
        IDS_POPUPS_BLOCKED_INFOBAR_TEXT, num_popups));
    return false;
  }

  on_show_popups_callback_ = std::move(on_show_popups_callback);
  url_ = GetWebContents().GetLastCommittedURL();
  // Unretained is safe because |this| will always outlive |message_| which owns
  // the callback.
  auto message = std::make_unique<messages::MessageWrapper>(
      messages::MessageIdentifier::POPUP_BLOCKED,
      base::BindOnce(&PopupBlockedMessageDelegate::HandleClick,
                     base::Unretained(this)),
      base::BindOnce(&PopupBlockedMessageDelegate::HandleDismissCallback,
                     base::Unretained(this)));

  message->SetTitle(l10n_util::GetPluralStringFUTF16(
      IDS_POPUPS_BLOCKED_INFOBAR_TEXT, num_popups));

  map_ = settings_map;
  allow_settings_changes_ = !PopupSettingManagedByPolicy(map_, url_);

  // Don't allow the user to configure the setting in the UI if the setting
  // is managed by policy.
  int button_text_id =
      allow_settings_changes_ ? IDS_POPUPS_BLOCKED_INFOBAR_BUTTON_SHOW : IDS_OK;
  message->SetPrimaryButtonText(l10n_util::GetStringUTF16(button_text_id));
  messages::MessageDispatcherBridge* message_dispatcher_bridge =
      messages::MessageDispatcherBridge::Get();
  DCHECK(message_dispatcher_bridge->IsMessagesEnabledForEmbedder());
  message->SetIconResourceId(message_dispatcher_bridge->MapToJavaDrawableId(
      IDR_ANDROID_INFOBAR_BLOCKED_POPUPS));

  // On rare occasions, such as the moment when activity is being recreated
  // or destroyed, popup blocked message will not be displayed and the
  // method will return false.
  if (!message_dispatcher_bridge->EnqueueMessage(
          message.get(), &GetWebContents(),
          messages::MessageScopeType::NAVIGATION,
          messages::MessagePriority::kNormal)) {
    return false;
  }

  message_ = std::move(message);
  return true;
}

PopupBlockedMessageDelegate::~PopupBlockedMessageDelegate() {
  if (message_ != nullptr) {
    messages::MessageDispatcherBridge::Get()->DismissMessage(
        message_.get(), messages::DismissReason::UNKNOWN);
  }
}

PopupBlockedMessageDelegate::PopupBlockedMessageDelegate(
    content::WebContents* web_contents)
    : content::WebContentsUserData<PopupBlockedMessageDelegate>(*web_contents) {
}

void PopupBlockedMessageDelegate::HandleDismissCallback(
    messages::DismissReason dismiss_reason) {
  message_.reset();
  map_ = nullptr;
}

void PopupBlockedMessageDelegate::HandleClick() {
  if (!allow_settings_changes_) {
    return;
  }

  // Create exceptions.
  map_->SetNarrowestContentSetting(url_, url_, ContentSettingsType::POPUPS,
                                   CONTENT_SETTING_ALLOW);

  // Launch popups.
  ShowBlockedPopups(&GetWebContents());

  if (on_show_popups_callback_)
    std::move(on_show_popups_callback_).Run();
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(PopupBlockedMessageDelegate);

}  // namespace blocked_content
