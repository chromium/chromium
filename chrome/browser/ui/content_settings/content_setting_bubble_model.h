// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_CONTENT_SETTINGS_CONTENT_SETTING_BUBBLE_MODEL_H_
#define CHROME_BROWSER_UI_CONTENT_SETTINGS_CONTENT_SETTING_BUBBLE_MODEL_H_

#include <stdint.h>

#include <map>
#include <set>
#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/scoped_observer.h"
#include "base/strings/string16.h"
#include "build/build_config.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/content_settings/tab_specific_content_settings.h"
#include "chrome/browser/ui/blocked_content/framebust_block_tab_helper.h"
#include "chrome/browser/ui/blocked_content/url_list_manager.h"
#include "chrome/common/custom_handlers/protocol_handler.h"
#include "chrome/grit/generated_resources.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "third_party/blink/public/common/mediastream/media_stream_request.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/image/image.h"
#include "url/gurl.h"

class ContentSettingBubbleModelDelegate;
class Profile;
class ProtocolHandlerRegistry;

namespace content {
class WebContents;
}

namespace rappor {
class RapporServiceImpl;
}

// The hierarchy of bubble models:
//
// ContentSettingBubbleModel                  - base class
//   ContentSettingSimpleBubbleModel             - single content setting
//     ContentSettingMixedScriptBubbleModel        - mixed script
//     ContentSettingRPHBubbleModel                - protocol handlers
//     ContentSettingMidiSysExBubbleModel          - midi sysex
//     ContentSettingDomainListBubbleModel         - domain list (geolocation)
//     ContentSettingPluginBubbleModel             - plugins
//     ContentSettingSingleRadioGroup              - radio group
//       ContentSettingCookiesBubbleModel            - cookies
//       ContentSettingPopupBubbleModel              - popups
//       ContentSettingFramebustBlockBubbleModel     - blocked frame busting
//   ContentSettingMediaStreamBubbleModel        - media (camera and mic)
//   ContentSettingSubresourceFilterBubbleModel  - filtered subresources
//   ContentSettingDownloadsBubbleModel          - automatic downloads
//   ContentSettingNotificationsBubbleModel      - notifications

// Forward declaration necessary for downcasts.
class ContentSettingSimpleBubbleModel;
class ContentSettingMediaStreamBubbleModel;
class ContentSettingSubresourceFilterBubbleModel;
class ContentSettingDownloadsBubbleModel;
class ContentSettingFramebustBlockBubbleModel;
class ContentSettingNotificationsBubbleModel;

// This model provides data for ContentSettingBubble, and also controls
// the action triggered when the allow / block radio buttons are triggered.
class ContentSettingBubbleModel {
 public:
  typedef ContentSettingBubbleModelDelegate Delegate;

  struct ListItem {
    ListItem(const gfx::VectorIcon* image,
             const base::string16& title,
             const base::string16& description,
             bool has_link,
             bool has_blocked_badge,
             int32_t item_id);
    ListItem(const ListItem& other);
    ListItem& operator=(const ListItem& other);
    const gfx::VectorIcon* image;
    base::string16 title;
    base::string16 description;
    bool has_link;
    bool has_blocked_badge;
    int32_t item_id;
  };
  typedef std::vector<ListItem> ListItems;

  class Owner {
   public:
    virtual void OnListItemAdded(const ListItem& item) {}
    virtual void OnListItemRemovedAt(int index) {}
    virtual int GetSelectedRadioOption() = 0;

   protected:
    virtual ~Owner() = default;
  };

  typedef std::vector<base::string16> RadioItems;
  struct RadioGroup {
    RadioGroup();
    ~RadioGroup();

    GURL url;
    RadioItems radio_items;
    int default_item;

    // Whether the user can control this radio group. False if controlled by
    // policy, etc.
    bool user_managed = true;
  };

  struct DomainList {
    DomainList();
    DomainList(const DomainList& other);
    ~DomainList();

    base::string16 title;
    std::set<std::string> hosts;
  };

  struct MediaMenu {
    MediaMenu();
    MediaMenu(const MediaMenu& other);
    ~MediaMenu();

    base::string16 label;
    blink::MediaStreamDevice default_device;
    blink::MediaStreamDevice selected_device;
    bool disabled;
  };
  typedef std::map<blink::mojom::MediaStreamType, MediaMenu> MediaMenuMap;

  enum class ManageTextStyle {
    // No Manage button or checkbox is displayed.
    kNone,
    // Manage text is displayed as a non-prominent button.
    kButton,
    // Manage text is used as a checkbox title.
    kCheckbox,
  };

  struct BubbleContent {
    BubbleContent();
    ~BubbleContent();

    base::string16 title;
    base::string16 message;
    ListItems list_items;
    RadioGroup radio_group;
    std::vector<DomainList> domain_lists;
    base::string16 custom_link;
    bool custom_link_enabled = false;
    base::string16 manage_text;
    ManageTextStyle manage_text_style = ManageTextStyle::kButton;
    MediaMenuMap media_menus;
    bool show_learn_more = false;
    base::string16 done_button_text;

   private:
    DISALLOW_COPY_AND_ASSIGN(BubbleContent);
  };

  static const int kAllowButtonIndex;

  // Creates a bubble model for a particular |content_type|. Note that not all
  // bubbles fit this description.
  // TODO(msramek): Move this to ContentSettingSimpleBubbleModel or remove
  // entirely.
  static std::unique_ptr<ContentSettingBubbleModel>
  CreateContentSettingBubbleModel(Delegate* delegate,
                                  content::WebContents* web_contents,
                                  ContentSettingsType content_type);

  virtual ~ContentSettingBubbleModel();

  const BubbleContent& bubble_content() const { return bubble_content_; }

  void set_owner(Owner* owner) { owner_ = owner; }

  virtual void OnListItemClicked(int index, int event_flags) {}
  virtual void OnCustomLinkClicked() {}
  virtual void OnManageButtonClicked() {}
  virtual void OnManageCheckboxChecked(bool is_checked) {}
  virtual void OnLearnMoreClicked() {}
  virtual void OnMediaMenuClicked(blink::mojom::MediaStreamType type,
                                  const std::string& selected_device_id) {}
  virtual void OnDoneButtonClicked() {}
  // Called by the view code when the bubble is closed
  virtual void CommitChanges() {}

  // TODO(msramek): The casting methods below are only necessary because
  // ContentSettingBubbleController in the Cocoa UI needs to know the type of
  // the bubble it wraps. Find a solution that does not require reflection nor
  // recreating the entire hierarchy for Cocoa UI.
  // Cast this bubble into ContentSettingSimpleBubbleModel if possible.
  virtual ContentSettingSimpleBubbleModel* AsSimpleBubbleModel();

  // Cast this bubble into ContentSettingMediaStreamBubbleModel if possible.
  virtual ContentSettingMediaStreamBubbleModel* AsMediaStreamBubbleModel();

  // Cast this bubble into ContentSettingSubresourceFilterBubbleModel
  // if possible.
  virtual ContentSettingSubresourceFilterBubbleModel*
  AsSubresourceFilterBubbleModel();

  // Cast this bubble into ContentSettingDownloadsBubbleModel if possible.
  virtual ContentSettingDownloadsBubbleModel* AsDownloadsBubbleModel();

  // Cast this bubble into ContentSettingFramebustBlockBubbleModel if possible.
  virtual ContentSettingFramebustBlockBubbleModel*
  AsFramebustBlockBubbleModel();

  // Cast this bubble into ContentSettingNotificationsBubbleModel if possible.
  virtual ContentSettingNotificationsBubbleModel* AsNotificationsBubbleModel();

  // Sets the Rappor service used for testing.
  void SetRapporServiceImplForTesting(
      rappor::RapporServiceImpl* rappor_service) {
    rappor_service_ = rappor_service;
  }

 protected:
  // |web_contents| must outlive this.
  ContentSettingBubbleModel(Delegate* delegate,
                            content::WebContents* web_contents);

  // Should always be non-nullptr.
  content::WebContents* web_contents() const { return web_contents_; }
  Profile* GetProfile() const;
  Delegate* delegate() const { return delegate_; }
  int selected_item() const { return owner_->GetSelectedRadioOption(); }

  void set_title(const base::string16& title) { bubble_content_.title = title; }
  void set_message(const base::string16& message) {
    bubble_content_.message = message;
  }
  void AddListItem(const ListItem& item);
  void RemoveListItem(int index);
  void set_radio_group(const RadioGroup& radio_group) {
    bubble_content_.radio_group = radio_group;
  }
  void add_domain_list(const DomainList& domain_list) {
    bubble_content_.domain_lists.push_back(domain_list);
  }
  void set_custom_link(const base::string16& link) {
    bubble_content_.custom_link = link;
  }
  void set_custom_link_enabled(bool enabled) {
    bubble_content_.custom_link_enabled = enabled;
  }
  void set_manage_text(const base::string16& text) {
    bubble_content_.manage_text = text;
  }
  void set_manage_text_style(ManageTextStyle manage_text_style) {
    bubble_content_.manage_text_style = manage_text_style;
  }
  void add_media_menu(blink::mojom::MediaStreamType type,
                      const MediaMenu& menu) {
    bubble_content_.media_menus[type] = menu;
  }
  void set_selected_device(const blink::MediaStreamDevice& device) {
    bubble_content_.media_menus[device.type].selected_device = device;
  }
  void set_show_learn_more(bool show_learn_more) {
    bubble_content_.show_learn_more = show_learn_more;
  }
  void set_done_button_text(const base::string16& done_button_text) {
    bubble_content_.done_button_text = done_button_text;
  }
  rappor::RapporServiceImpl* rappor_service() const { return rappor_service_; }

 private:
  content::WebContents* web_contents_;
  Owner* owner_;
  Delegate* delegate_;
  BubbleContent bubble_content_;
  // The service used to record Rappor metrics. Can be set for testing.
  rappor::RapporServiceImpl* rappor_service_;

  DISALLOW_COPY_AND_ASSIGN(ContentSettingBubbleModel);
};

// A generic bubble used for a single content setting.
class ContentSettingSimpleBubbleModel : public ContentSettingBubbleModel {
 public:
  ContentSettingSimpleBubbleModel(Delegate* delegate,
                                  content::WebContents* web_contents,
                                  ContentSettingsType content_type);

  ContentSettingsType content_type() { return content_type_; }

  // ContentSettingBubbleModel implementation.
  ContentSettingSimpleBubbleModel* AsSimpleBubbleModel() override;

 private:
  FRIEND_TEST_ALL_PREFIXES(FramebustBlockBrowserTest, ManageButtonClicked);

  // ContentSettingBubbleModel implementation.
  void SetTitle();
  void SetMessage();
  void SetManageText();
  void OnManageButtonClicked() override;
  void SetCustomLink();
  void OnCustomLinkClicked() override;

  ContentSettingsType content_type_;

  DISALLOW_COPY_AND_ASSIGN(ContentSettingSimpleBubbleModel);
};

// RPH stands for Register Protocol Handler.
class ContentSettingRPHBubbleModel : public ContentSettingSimpleBubbleModel {
 public:
  ContentSettingRPHBubbleModel(Delegate* delegate,
                               content::WebContents* web_contents,
                               ProtocolHandlerRegistry* registry);
  ~ContentSettingRPHBubbleModel() override;

  // ContentSettingBubbleModel:
  void CommitChanges() override;

 private:
  void RegisterProtocolHandler();
  void UnregisterProtocolHandler();
  void IgnoreProtocolHandler();
  void ClearOrSetPreviousHandler();
  void PerformActionForSelectedItem();

  ProtocolHandlerRegistry* registry_;
  ProtocolHandler pending_handler_;
  ProtocolHandler previous_handler_;

  DISALLOW_COPY_AND_ASSIGN(ContentSettingRPHBubbleModel);
};

// The model of the content settings bubble for media settings.
class ContentSettingMediaStreamBubbleModel : public ContentSettingBubbleModel {
 public:
  ContentSettingMediaStreamBubbleModel(Delegate* delegate,
                                       content::WebContents* web_contents);

  ~ContentSettingMediaStreamBubbleModel() override;

  // ContentSettingBubbleModel:
  ContentSettingMediaStreamBubbleModel* AsMediaStreamBubbleModel() override;
  void CommitChanges() override;
  void OnManageButtonClicked() override;
  void OnDoneButtonClicked() override;

 private:
  // Helper functions to check if this bubble was invoked for microphone,
  // camera, or both devices.
  bool MicrophoneAccessed() const;
  bool CameraAccessed() const;

  bool MicrophoneBlocked() const;
  bool CameraBlocked() const;

  void SetTitle();
  void SetMessage();
  void SetManageText();

  // Sets the data for the radio buttons of the bubble.
  void SetRadioGroup();

  // Sets the data for the media menus of the bubble.
  void SetMediaMenus();

  // Sets the string that suggests reloading after the settings were changed.
  void SetCustomLink();

  // Updates the camera and microphone setting with the passed |setting|.
  void UpdateSettings(ContentSetting setting);

#if defined(OS_MACOSX)
  // Initialize the bubble with the elements specific to the scenario when
  // camera or mic are disabled in a system (OS) level.
  void InitializeSystemMediaPermissionBubble();
#endif  // defined(OS_MACOSX)

  // Whether or not to show the bubble UI specific to when media permissions are
  // turned off in a system level.
  bool ShouldShowSystemMediaPermissions();

  // Updates the camera and microphone default device with the passed |type|
  // and device.
  void UpdateDefaultDeviceForType(blink::mojom::MediaStreamType type,
                                  const std::string& device);

  // ContentSettingBubbleModel implementation.
  void OnMediaMenuClicked(blink::mojom::MediaStreamType type,
                          const std::string& selected_device) override;

  // The content settings that are associated with the individual radio
  // buttons.
  ContentSetting radio_item_setting_[2];
  // The state of the microphone and camera access.
  TabSpecificContentSettings::MicrophoneCameraState state_;

  DISALLOW_COPY_AND_ASSIGN(ContentSettingMediaStreamBubbleModel);
};

// The model of a bubble that acts as a quiet permission request prompt for
// notifications. In contrast to other bubbles (which display the current
// permission state after the user makes the initial decision), this is shown
// before the user makes the first ever permission decisions.
class ContentSettingNotificationsBubbleModel
    : public ContentSettingBubbleModel {
 public:
  ContentSettingNotificationsBubbleModel(Delegate* delegate,
                                         content::WebContents* web_contents);

  ~ContentSettingNotificationsBubbleModel() override;

 private:
  void SetManageText();

  // ContentSettingBubbleModel:
  void OnManageButtonClicked() override;
  void OnDoneButtonClicked() override;
  ContentSettingNotificationsBubbleModel* AsNotificationsBubbleModel() override;

  DISALLOW_COPY_AND_ASSIGN(ContentSettingNotificationsBubbleModel);
};

// The model for the deceptive content bubble.
class ContentSettingSubresourceFilterBubbleModel
    : public ContentSettingBubbleModel {
 public:
  ContentSettingSubresourceFilterBubbleModel(
      Delegate* delegate,
      content::WebContents* web_contents);

  ~ContentSettingSubresourceFilterBubbleModel() override;

 private:
  void SetMessage();
  void SetTitle();
  void SetManageText();

  // ContentSettingBubbleModel:
  void OnManageCheckboxChecked(bool is_checked) override;
  ContentSettingSubresourceFilterBubbleModel* AsSubresourceFilterBubbleModel()
      override;
  void OnLearnMoreClicked() override;
  void CommitChanges() override;

  bool is_checked_ = false;

  DISALLOW_COPY_AND_ASSIGN(ContentSettingSubresourceFilterBubbleModel);
};

// The model for automatic downloads setting.
class ContentSettingDownloadsBubbleModel : public ContentSettingBubbleModel {
 public:
  ContentSettingDownloadsBubbleModel(Delegate* delegate,
                                     content::WebContents* web_contents);
  ~ContentSettingDownloadsBubbleModel() override;

  // ContentSettingBubbleModel overrides:
  ContentSettingDownloadsBubbleModel* AsDownloadsBubbleModel() override;
  void CommitChanges() override;

 private:
  void SetRadioGroup();
  void SetTitle();
  void SetManageText();

  // ContentSettingBubbleModel overrides:
  void OnManageButtonClicked() override;

  DISALLOW_COPY_AND_ASSIGN(ContentSettingDownloadsBubbleModel);
};

class ContentSettingSingleRadioGroup : public ContentSettingSimpleBubbleModel {
 public:
  ContentSettingSingleRadioGroup(Delegate* delegate,
                                 content::WebContents* web_contents,
                                 ContentSettingsType content_type);
  ~ContentSettingSingleRadioGroup() override;

  // ContentSettingSimpleBubbleModel:
  void CommitChanges() override;

 protected:
  bool settings_changed() const;

 private:
  FRIEND_TEST_ALL_PREFIXES(FramebustBlockBrowserTest, AllowRadioButtonSelected);
  FRIEND_TEST_ALL_PREFIXES(FramebustBlockBrowserTest,
                           DisallowRadioButtonSelected);

  void SetRadioGroup();
  void SetNarrowestContentSetting(ContentSetting setting);

  ContentSetting block_setting_;

  DISALLOW_COPY_AND_ASSIGN(ContentSettingSingleRadioGroup);
};

#if !defined(OS_ANDROID)
// The model for the blocked Framebust bubble.
class ContentSettingFramebustBlockBubbleModel
    : public ContentSettingSingleRadioGroup,
      public UrlListManager::Observer {
 public:
  ContentSettingFramebustBlockBubbleModel(Delegate* delegate,
                                          content::WebContents* web_contents);

  ~ContentSettingFramebustBlockBubbleModel() override;

  // ContentSettingBubbleModel:
  void OnListItemClicked(int index, int event_flags) override;
  ContentSettingFramebustBlockBubbleModel* AsFramebustBlockBubbleModel()
      override;

  // UrlListManager::Observer:
  void BlockedUrlAdded(int32_t id, const GURL& blocked_url) override;

 private:
  ListItem CreateListItem(const GURL& url);

  ScopedObserver<UrlListManager, UrlListManager::Observer> url_list_observer_;

  DISALLOW_COPY_AND_ASSIGN(ContentSettingFramebustBlockBubbleModel);
};
#endif  // !defined(OS_ANDROID)

#endif  // CHROME_BROWSER_UI_CONTENT_SETTINGS_CONTENT_SETTING_BUBBLE_MODEL_H_
