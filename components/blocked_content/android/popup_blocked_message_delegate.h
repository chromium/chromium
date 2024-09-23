// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BLOCKED_CONTENT_ANDROID_POPUP_BLOCKED_MESSAGE_DELEGATE_H_
#define COMPONENTS_BLOCKED_CONTENT_ANDROID_POPUP_BLOCKED_MESSAGE_DELEGATE_H_

#include <memory>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "components/messages/android/message_enums.h"
#include "components/messages/android/message_wrapper.h"
#include "content/public/browser/web_contents_user_data.h"
#include "url/gurl.h"

class HostContentSettingsMap;

namespace content {
class WebContents;
}  // namespace content

namespace blocked_content {

// A popup blocked delegate responsible for showing message bubbles.
// Created lazily when a popup is first blocked, and matches the
// lifetime of WebContents afterwards.
class PopupBlockedMessageDelegate
    : public content::WebContentsUserData<PopupBlockedMessageDelegate> {
 public:
  bool ShowMessage(int num_popups,
                   HostContentSettingsMap* settings_map,
                   base::OnceClosure on_accept_callback);

  ~PopupBlockedMessageDelegate() override;

  messages::MessageWrapper* message_for_testing() { return message_.get(); }

 private:
  friend class content::WebContentsUserData<PopupBlockedMessageDelegate>;

  explicit PopupBlockedMessageDelegate(content::WebContents* web_contents);
  void HandleClick();
  void HandleDismissCallback(messages::DismissReason dismiss_reason);

  raw_ptr<HostContentSettingsMap> map_ = nullptr;

  // TODO(crbug.com/40749729): considering grouping the following members into a
  // struct because they all logically match the lifetime of a single Message.
  GURL url_;
  bool allow_settings_changes_ = false;
  base::OnceClosure on_show_popups_callback_;
  std::unique_ptr<messages::MessageWrapper> message_;

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

}  // namespace blocked_content

#endif  // COMPONENTS_BLOCKED_CONTENT_ANDROID_POPUP_BLOCKED_MESSAGE_DELEGATE_H_
