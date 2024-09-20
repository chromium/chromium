// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_CONTENT_SETTINGS_CONTENT_SETTING_BUBBLE_MODEL_H_
#define CHROME_BROWSER_UI_CONTENT_SETTINGS_CONTENT_SETTING_BUBBLE_MODEL_H_

#include <stdint.h>

#include <map>
#include <string>
#include <vector>

#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "build/build_config.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ui/blocked_content/framebust_block_tab_helper.h"
#include "components/blocked_content/url_list_manager.h"
#include "components/content_settings/browser/page_specific_content_settings.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/custom_handlers/protocol_handler.h"
#include "net/base/schemeful_site.h"
#include "services/device/public/cpp/geolocation/buildflags.h"
#include "third_party/blink/public/common/mediastream/media_stream_request.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/image/image.h"
#include "url/gurl.h"
#include "url/origin.h"

class ContentSettingBubbleModelDelegate;
class Profile;
namespace custom_handlers {
class ProtocolHandlerRegistry;
}

namespace content {
class Page;
class WebContents;
}

namespace ui {
class Event;
}

// The hierarchy of bubble models:
//
// ContentSettingBubbleModel                  - base class
//   ContentSettingSimpleBubbleModel             - single content setting
//     ContentSettingMixedScriptBubbleModel        - mixed script
//     ContentSettingRPHBubbleModel                - protocol handlers
//     ContentSettingPluginBubbleModel             - plugins
//     ContentSettingSingleRadioGroup              - radio group
//       ContentSettingCookiesBubbleModel            - cookies
//       ContentSettingPopupBubbleModel              - popups
//       ContentSettingFramebustBlockBubbleModel     - blocked frame busting
//   ContentSettingMediaStreamBubbleModel        - media (camera and mic)
//   ContentSettingSubresourceFilterBubbleModel  - filtered subresources
//   ContentSettingDownloadsBubbleModel          - automatic downloads
//   ContentSettingQuietRequestBubbleModel       - quiet ui prompts
//   ContentSettingStorageAccessBubbleModel      - saa prompts

// Forward declaration necessary for downcasts.
class ContentSettingSimpleBubbleModel;
class ContentSettingMediaStreamBubbleModel;
class ContentSettingSubresourceFilterBubbleModel;
class ContentSettingDownloadsBubbleModel;
class ContentSettingFramebustBlockBubbleModel;
class ContentSettingQuietRequestBubbleModel;

// This model provides data for ContentSettingBubble, and also controls
// the action triggered when the allow / block radio buttons are triggered.
class ContentSettingBubbleModel {
 public:
  typedef ContentSettingBubbleModelDelegate Delegate;

  struct ListItem {
    ListItem(const gfx::VectorIcon* image,
             const std::u16string& title,
             const std::u16string& description,
             bool has_link,
             bool has_blocked_badge,
             int32_t item_id);
    ListItem(const ListItem& other);
    ListItem& operator=(const ListItem& other);
    raw_ptr<const gfx::VectorIcon> image;
    std::u16string title;
    std::u16string description;
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

  typedef std::vector<std::u16string> RadioItems;
  struct RadioGroup {
    RadioGroup();
    ~RadioGroup();

    GURL url;
    RadioItems radio_items;
    int default_item = 0;
  };

  typedef std::map<net::SchemefulSite, /*allowed*/ bool> SiteList;

  struct MediaMenu {
    MediaMenu();
    MediaMenu(const MediaMenu& other);
    ~MediaMenu();

    std::u16string label;
    blink::MediaStreamDevice default_device;
    blink::MediaStreamDevice selected_device;
    bool disabled = false;
  };
  typedef std::map<blink::mojom::MediaStreamType, MediaMenu> MediaMenuMap;

  enum class ManageTextStyle {
    // No Manage button or checkbox is displayed.
    kNone,
    // Manage text is displayed as a non-prominent button.
    kButton,
    // Manage text is used as a checkbox title.
    kCheckbox,
    // Manage text is shown in a HoverButton. The "Manage" and "Done" buttons
    // are hidden.
    kHoverButton,
  };

  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  enum class ContentSettingBubbleAction {
    kOpened = 1,
    kPermissionAllowed = 2,
    kPermissionBlocked = 3,
    kManageButtonClicked = 4,

    kMaxValue = kManageButtonClicked
  };

  struct BubbleContent {
    BubbleContent();

    BubbleContent(const BubbleContent&) = delete;
    BubbleContent& operator=(const BubbleContent&) = delete;

    ~BubbleContent();

    std::u16string title;
    std::u16string subtitle;
    std::u16string message;
    // Whether the user can modify the content of the bubble.
    // False if controlled by policy, etc.
    bool is_user_modifiable = true;
    ListItems list_items;
    RadioGroup radio_group;
    SiteList site_list;
    std::u16string custom_link;
    bool custom_link_enabled = false;
    std::u16string manage_text;
    std::u16string manage_tooltip;
    ManageTextStyle manage_text_style = ManageTextStyle::kButton;
    bool show_learn_more = false;
    std::u16string done_button_text;
    std::u16string cancel_button_text;
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

  ContentSettingBubbleModel(const ContentSettingBubbleModel&) = delete;
  ContentSettingBubbleModel& operator=(const ContentSettingBubbleModel&) =
      delete;

  virtual ~ContentSettingBubbleModel();

  const BubbleContent& bubble_content() const { return bubble_content_; }

  void set_owner(Owner* owner) { owner_ = owner; }

  virtual void OnListItemClicked(int index, const ui::Event& event) {}
  virtual void OnSiteRowClicked(const net::SchemefulSite& site,
                                bool is_allowed) {}
  virtual void OnCustomLinkClicked() {}
  virtual void OnManageButtonClicked() {}
  virtual void OnManageCheckboxChecked(bool is_checked) {}
  virtual void OnLearnMoreClicked() {}
  virtual void OnDoneButtonClicked() {}
  virtual void OnCancelButtonClicked() {}
  // Called by the view code when the bubble is closed.
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

  // Cast this bubble into ContentSettingQuietRequestBubbleModel if possible.
  virtual ContentSettingQuietRequestBubbleModel* AsQuietRequestBubbleModel();

  // Overrides the display URL used in the content bubble UI.
  static base::AutoReset<std::optional<bool>>
  CreateScopedDisplayURLOverrideForTesting();

  bool is_UMA_for_test = false;

 protected:
  // |web_contents| must outlive this.
  ContentSettingBubbleModel(Delegate* delegate,
                            content::WebContents* web_contents);

  // Should always be non-nullptr.
  content::WebContents* web_contents() const { return web_contents_; }
  Profile* GetProfile() const;
  Delegate* delegate() const { return delegate_; }
  int selected_item() const { return owner_->GetSelectedRadioOption(); }
  content::Page& GetPage() const { return web_contents_->GetPrimaryPage(); }

  void set_title(const std::u16string& title) { bubble_content_.title = title; }
  void set_subtitle(const std::u16string& subtitle) {
    bubble_content_.subtitle = subtitle;
  }
  void set_message(const std::u16string& message) {
    bubble_content_.message = message;
  }
  void clear_message() { bubble_content_.message.clear(); }
  void AddListItem(const ListItem& item);
  void RemoveListItem(int index);
  void set_radio_group(const RadioGroup& radio_group) {
    bubble_content_.radio_group = radio_group;
  }
  void set_site_list(const SiteList& site_list) {
    bubble_content_.site_list = site_list;
  }
  void set_custom_link(const std::u16string& link) {
    bubble_content_.custom_link = link;
  }
  void set_custom_link_enabled(bool enabled) {
    bubble_content_.custom_link_enabled = enabled;
  }
  void set_manage_text(const std::u16string& text) {
    bubble_content_.manage_text = text;
  }
  void set_manage_tooltip(const std::u16string& text) {
    bubble_content_.manage_tooltip = text;
  }
  void set_manage_text_style(ManageTextStyle manage_text_style) {
    bubble_content_.manage_text_style = manage_text_style;
  }
  void set_show_learn_more(bool show_learn_more) {
    bubble_content_.show_learn_more = show_learn_more;
  }
  void set_done_button_text(const std::u16string& done_button_text) {
    bubble_content_.done_button_text = done_button_text;
  }
  void set_cancel_button_text(const std::u16string& cancel_button_text) {
    bubble_content_.cancel_button_text = cancel_button_text;
  }
  void set_is_user_modifiable(bool is_user_modifiable) {
    bubble_content_.is_user_modifiable = is_user_modifiable;
  }

 private:
  raw_ptr<content::WebContents, DanglingUntriaged> web_contents_;
  raw_ptr<Owner, DanglingUntriaged> owner_;
  raw_ptr<Delegate> delegate_;
  BubbleContent bubble_content_;
};

// A generic bubble used for a single content setting.
class ContentSettingSimpleBubbleModel : public ContentSettingBubbleModel {
 public:
  ContentSettingSimpleBubbleModel(Delegate* delegate,
                                  content::WebContents* web_contents,
                                  ContentSettingsType content_type);

  ContentSettingSimpleBubbleModel(const ContentSettingSimpleBubbleModel&) =
      delete;
  ContentSettingSimpleBubbleModel& operator=(
      const ContentSettingSimpleBubbleModel&) = delete;

  ContentSettingsType content_type() { return content_type_; }

  // ContentSettingBubbleModel implementation.
  ContentSettingSimpleBubbleModel* AsSimpleBubbleModel() override;

 protected:
  bool IsContentAllowed();

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
};

// RPH stands for Register Protocol Handler.
class ContentSettingRPHBubbleModel : public ContentSettingSimpleBubbleModel {
 public:
  ContentSettingRPHBubbleModel(
      Delegate* delegate,
      content::WebContents* web_contents,
      custom_handlers::ProtocolHandlerRegistry* registry);

  ContentSettingRPHBubbleModel(const ContentSettingRPHBubbleModel&) = delete;
  ContentSettingRPHBubbleModel& operator=(const ContentSettingRPHBubbleModel&) =
      delete;

  ~ContentSettingRPHBubbleModel() override;

  // ContentSettingBubbleModel:
  void CommitChanges() override;

 private:
  void RegisterProtocolHandler();
  void UnregisterProtocolHandler();
  void IgnoreProtocolHandler();
  void ClearOrSetPreviousHandler();
  void PerformActionForSelectedItem();

  raw_ptr<custom_handlers::ProtocolHandlerRegistry> registry_;
  custom_handlers::ProtocolHandler pending_handler_;
  custom_handlers::ProtocolHandler previous_handler_;
};

// The model of the content settings bubble for media settings.
class ContentSettingMediaStreamBubbleModel : public ContentSettingBubbleModel {
 public:
  ContentSettingMediaStreamBubbleModel(Delegate* delegate,
                                       content::WebContents* web_contents);

  ContentSettingMediaStreamBubbleModel(
      const ContentSettingMediaStreamBubbleModel&) = delete;
  ContentSettingMediaStreamBubbleModel& operator=(
      const ContentSettingMediaStreamBubbleModel&) = delete;

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

  // Sets whether microphone, camera, or both device permissions can be
  // modified.
  void SetIsUserModifiable();

  // Updates the camera and microphone setting with the passed |setting|.
  void UpdateSettings(ContentSetting setting);

#if BUILDFLAG(IS_MAC)
  // Initialize the bubble with the elements specific to the scenario when
  // camera or mic are disabled in a system (OS) level.
  void InitializeSystemMediaPermissionBubble();
#endif  // BUILDFLAG(IS_MAC)

  // Whether or not to show the bubble UI specific to when media permissions are
  // turned off in a system level.
  bool ShouldShowSystemMediaPermissions();

  // Updates the camera and microphone default device with the passed |type|
  // and device.
  void UpdateDefaultDeviceForType(blink::mojom::MediaStreamType type,
                                  const std::string& device);

  // The content settings that are associated with the individual radio
  // buttons.
  ContentSetting radio_item_setting_[2];
  // The state of the microphone and camera access.
  content_settings::PageSpecificContentSettings::MicrophoneCameraState state_;
};

// The model of a bubble that acts as a quiet permission request prompt for
// the notification and geolocation permissions. In contrast to other bubbles
// (which display the current permission state after the user makes the initial
// decision), this is shown before the user makes the first ever permission
// decisions.
class ContentSettingQuietRequestBubbleModel : public ContentSettingBubbleModel {
 public:
  ContentSettingQuietRequestBubbleModel(Delegate* delegate,
                                        content::WebContents* web_contents);

  ContentSettingQuietRequestBubbleModel(
      const ContentSettingQuietRequestBubbleModel&) = delete;
  ContentSettingQuietRequestBubbleModel& operator=(
      const ContentSettingQuietRequestBubbleModel&) = delete;

  ~ContentSettingQuietRequestBubbleModel() override;

 private:
  void SetManageText();

  // ContentSettingBubbleModel:
  void OnManageButtonClicked() override;
  void OnLearnMoreClicked() override;
  void OnDoneButtonClicked() override;
  void OnCancelButtonClicked() override;
  ContentSettingQuietRequestBubbleModel* AsQuietRequestBubbleModel() override;
};

// The model for the deceptive content bubble.
class ContentSettingSubresourceFilterBubbleModel
    : public ContentSettingBubbleModel {
 public:
  ContentSettingSubresourceFilterBubbleModel(
      Delegate* delegate,
      content::WebContents* web_contents);

  ContentSettingSubresourceFilterBubbleModel(
      const ContentSettingSubresourceFilterBubbleModel&) = delete;
  ContentSettingSubresourceFilterBubbleModel& operator=(
      const ContentSettingSubresourceFilterBubbleModel&) = delete;

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
};

// The model for automatic downloads setting.
class ContentSettingDownloadsBubbleModel : public ContentSettingBubbleModel {
 public:
  ContentSettingDownloadsBubbleModel(Delegate* delegate,
                                     content::WebContents* web_contents);

  ContentSettingDownloadsBubbleModel(
      const ContentSettingDownloadsBubbleModel&) = delete;
  ContentSettingDownloadsBubbleModel& operator=(
      const ContentSettingDownloadsBubbleModel&) = delete;

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
};

class ContentSettingSingleRadioGroup : public ContentSettingSimpleBubbleModel {
 public:
  ContentSettingSingleRadioGroup(Delegate* delegate,
                                 content::WebContents* web_contents,
                                 ContentSettingsType content_type);

  ContentSettingSingleRadioGroup(const ContentSettingSingleRadioGroup&) =
      delete;
  ContentSettingSingleRadioGroup& operator=(
      const ContentSettingSingleRadioGroup&) = delete;

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
};

// The bubble that allows users to control StorageAccess permission.
// It uses checkboxes instead of radio buttons to allow users to control
// multiple embedded sites.
class ContentSettingStorageAccessBubbleModel
    : public ContentSettingBubbleModel {
 public:
  ContentSettingStorageAccessBubbleModel(Delegate* delegate,
                                         content::WebContents* web_contents);
  ~ContentSettingStorageAccessBubbleModel() override;

  ContentSettingStorageAccessBubbleModel(
      const ContentSettingStorageAccessBubbleModel&) = delete;
  ContentSettingStorageAccessBubbleModel& operator=(
      const ContentSettingStorageAccessBubbleModel&) = delete;

  // ContentSettingBubbleModel:
  void OnManageButtonClicked() override;
  void CommitChanges() override;
  void OnSiteRowClicked(const net::SchemefulSite& site,
                        bool is_allowed) override;

 private:
  std::map<net::SchemefulSite, /*is_allowed*/ bool> changed_permissions_;
};

// The bubble that informs users that Chrome does not have access to Location
// and guides them to the system preferences to fix that problem if they wish.
class ContentSettingGeolocationBubbleModel
    : public ContentSettingSingleRadioGroup {
 public:
  ContentSettingGeolocationBubbleModel(Delegate* delegate,
                                       content::WebContents* web_contents);

  ContentSettingGeolocationBubbleModel(
      const ContentSettingGeolocationBubbleModel&) = delete;
  ContentSettingGeolocationBubbleModel& operator=(
      const ContentSettingGeolocationBubbleModel&) = delete;

  ~ContentSettingGeolocationBubbleModel() override;

  // ContentSettingBubbleModel:
  void OnManageButtonClicked() override;
  void OnDoneButtonClicked() override;
  void CommitChanges() override;

 private:
  // Initialize the bubble with the elements specific to the scenario when
  // geolocation is disabled on the system (OS) level.
  void InitializeSystemGeolocationPermissionBubble();

  void SetCustomLink();

  // Whether or not we are showing the bubble UI specific to when geolocation
  // permissions are turned off on a system level.
  bool show_system_geolocation_bubble_ = false;
};

#if BUILDFLAG(IS_MAC)
// The bubble that informs users that the app does not have access to
// Notifications and guides them to the system settings to fix that problem
// if they wish.
class ContentSettingNotificationsBubbleModel
    : public ContentSettingSimpleBubbleModel {
 public:
  ContentSettingNotificationsBubbleModel(Delegate* delegate,
                                         content::WebContents* web_contents);

  ContentSettingNotificationsBubbleModel(
      const ContentSettingNotificationsBubbleModel&) = delete;
  ContentSettingNotificationsBubbleModel& operator=(
      const ContentSettingNotificationsBubbleModel&) = delete;

  ~ContentSettingNotificationsBubbleModel() override;

  // ContentSettingBubbleModel:
  void OnDoneButtonClicked() override;
};
#endif

#if !BUILDFLAG(IS_ANDROID)
// The model for the blocked Framebust bubble.
class ContentSettingFramebustBlockBubbleModel
    : public ContentSettingSingleRadioGroup,
      public blocked_content::UrlListManager::Observer {
 public:
  ContentSettingFramebustBlockBubbleModel(Delegate* delegate,
                                          content::WebContents* web_contents);

  ContentSettingFramebustBlockBubbleModel(
      const ContentSettingFramebustBlockBubbleModel&) = delete;
  ContentSettingFramebustBlockBubbleModel& operator=(
      const ContentSettingFramebustBlockBubbleModel&) = delete;

  ~ContentSettingFramebustBlockBubbleModel() override;

  // ContentSettingBubbleModel:
  void OnListItemClicked(int index, const ui::Event& event) override;
  ContentSettingFramebustBlockBubbleModel* AsFramebustBlockBubbleModel()
      override;

  // UrlListManager::Observer:
  void BlockedUrlAdded(int32_t id, const GURL& blocked_url) override;

 private:
  ListItem CreateListItem(const GURL& url);

  base::ScopedObservation<blocked_content::UrlListManager,
                          blocked_content::UrlListManager::Observer>
      url_list_observation_{this};
};
#endif  // !BUILDFLAG(IS_ANDROID)

#endif  // CHROME_BROWSER_UI_CONTENT_SETTINGS_CONTENT_SETTING_BUBBLE_MODEL_H_
