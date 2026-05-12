// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_SEND_TAB_TO_SELF_SEND_TAB_TO_SELF_BUBBLE_CONTROLLER_H_
#define CHROME_BROWSER_UI_VIEWS_SEND_TAB_TO_SELF_SEND_TAB_TO_SELF_BUBBLE_CONTROLLER_H_

#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "components/send_tab_to_self/entry_point_display_reason.h"
#include "components/send_tab_to_self/metrics_util.h"
#include "content/public/browser/web_contents_user_data.h"
#include "ui/views/widget/widget_observer.h"
#include "url/gurl.h"

class Profile;

namespace actions {
class ActionItem;
}

namespace views {
class Widget;
}

namespace content {
class WebContents;
}  // namespace content

namespace ui {
class Event;
}  // namespace ui

namespace user_prefs {
class PrefRegistrySyncable;
}  // namespace user_prefs

struct AccountInfo;

namespace send_tab_to_self {

enum class SendTabToSelfResult;
class SendTabToSelfBubbleView;
struct TargetDeviceInfo;

class SendTabToSelfBubbleController
    : public content::WebContentsUserData<SendTabToSelfBubbleController>,
      public views::WidgetObserver {
 public:
  SendTabToSelfBubbleController(const SendTabToSelfBubbleController&) = delete;
  SendTabToSelfBubbleController& operator=(
      const SendTabToSelfBubbleController&) = delete;

  ~SendTabToSelfBubbleController() override;

  // Hides send tab to self bubble.
  void HideBubble();
  // Displays send tab to self bubble.
  void ShowBubble(bool show_back_button = false);

  bool IsBubbleShown() const;

  // Returns nullptr if no bubble is currently shown.
  SendTabToSelfBubbleView* send_tab_to_self_bubble_view() const;
  // Returns the valid devices info map.
  virtual std::vector<TargetDeviceInfo> GetValidDevices();

  virtual AccountInfo GetSharingAccountInfo();

  // Handles the action when the user click on one valid device. Sends tab to
  // the target device.
  // Virtual for testing.
  virtual void OnDeviceSelected(const std::string& target_device_guid,
                                std::string_view device_name);

  // Handler for when user clicks the link to manage their available devices.
  void OnManageDevicesClicked(const ui::Event& event);

  // Close the bubble when the user clicks on the back button.
  void OnBackButtonPressed();

  // Returns true if the initial "Send" animation that's displayed once per
  // profile was shown.
  bool InitialSendAnimationShown();
  void SetInitialSendAnimationShown(bool shown);

  bool show_back_button() const { return show_back_button_; }

  base::WeakPtr<SendTabToSelfBubbleController> AsWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

  // Register SendTabToSelfBubbleController related prefs in the Profile prefs.
  static void RegisterProfilePrefs(
      user_prefs::PrefRegistrySyncable* user_prefs);

  void SetSelectorGenerationTimeoutForTesting(base::TimeDelta timeout);

 protected:
  explicit SendTabToSelfBubbleController(content::WebContents* web_contents);

 private:
  friend class content::WebContentsUserData<SendTabToSelfBubbleController>;

  Profile* GetProfile();
  virtual std::optional<EntryPointDisplayReason> GetEntryPointDisplayReason();

  void HandleSendTabToDeviceResult(const GURL& url,
                                   std::string_view device_name,
                                   SendTabToSelfResult result);

  // views::WidgetObserver:
  void OnWidgetDestroying(views::Widget* widget) override;

  // Weak reference. Will be nullptr if no bubble is currently shown.
  raw_ptr<SendTabToSelfBubbleView> send_tab_to_self_bubble_view_ = nullptr;
  // True if the back button is currently shown.
  bool show_back_button_ = false;

  raw_ptr<actions::ActionItem> send_tab_to_self_action_item_ = nullptr;

  base::ScopedObservation<views::Widget, views::WidgetObserver>
      widget_observation_{this};

  base::WeakPtrFactory<SendTabToSelfBubbleController> weak_ptr_factory_{this};

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

}  // namespace send_tab_to_self

#endif  // CHROME_BROWSER_UI_VIEWS_SEND_TAB_TO_SELF_SEND_TAB_TO_SELF_BUBBLE_CONTROLLER_H_
