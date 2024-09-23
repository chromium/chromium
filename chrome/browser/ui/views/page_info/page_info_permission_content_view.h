// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PAGE_INFO_PAGE_INFO_PERMISSION_CONTENT_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_PAGE_INFO_PAGE_INFO_PERMISSION_CONTENT_VIEW_H_

#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "components/page_info/page_info_ui.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/view.h"

#if !BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/ui/views/media_preview/page_info_previews_coordinator.h"
#include "components/media_effects/media_device_info.h"
#endif

class ChromePageInfoUiDelegate;
class NonAccessibleImageView;

namespace views {
class Checkbox;
class Label;
class ToggleButton;
}  // namespace views

// The view that is used as a content view of the permissions subpages in page
// info. It contains information about the permission (icon, title, state label)
// and controls to change the permission state (toggle, checkbox and manage
// button).
// *---------------------------------------------------------------*
// | Icon | Title                                         | Toggle |
// |      | State label                                   |        |
// |      |                                               |        |
// |      | "Remember this setting" checkbox              |        |
// |---------------------------------------------------------------|
// | Manage button                                                 |
// *---------------------------------------------------------------*
// The view for the File System permission subpage additionally contains a
// scrollable panel listing the files and/or directories with granted,
// active permissions.
// *---------------------------------------------------------------*
// | Icon | Title                                         | Toggle |
// |      | State label                                   |        |
// |      |--------------------------------------------------------|
// |      | Scrollable panel of files / directories                |
// |      |                                                        |
// |      | "Remember this setting" checkbox                       |
// |---------------------------------------------------------------|
// | Manage button                                                 |
// *---------------------------------------------------------------*
class PageInfoPermissionContentView
    : public views::View,
#if !BUILDFLAG(IS_CHROMEOS)
      public media_effects::MediaDeviceInfo::Observer,
#endif
      public PageInfoUI {
  METADATA_HEADER(PageInfoPermissionContentView, views::View)

 public:
  PageInfoPermissionContentView(PageInfo* presenter,
                                ChromePageInfoUiDelegate* ui_delegate,
                                ContentSettingsType type,
                                content::WebContents* web_contents);
  ~PageInfoPermissionContentView() override;

  // PageInfoUI implementations.
  void SetPermissionInfo(const PermissionInfoList& permission_info_list,
                         ChosenObjectInfoList chosen_object_info_list) override;

#if !BUILDFLAG(IS_CHROMEOS)
  const raw_ptr<views::Label> GetTitleForTesting() const { return title_; }

  const std::optional<PageInfoPreviewsCoordinator>&
  GetPreviewsCoordinatorForTesting() const {
    return previews_coordinator_;
  }
#endif

 private:
  // views::View overrides
  void ChildPreferredSizeChanged(views::View* child) override;

  void OnToggleButtonPressed();
  void OnRememberSettingPressed();
  void PermissionChanged();
  void ToggleFileSystemExtendedPermissions();

#if !BUILDFLAG(IS_CHROMEOS)
  // media_effects::MediaDeviceInfo::Observer overrides.
  void OnAudioDevicesChanged(
      const std::optional<std::vector<media::AudioDeviceDescription>>&
          device_infos) override;
  void OnVideoDevicesChanged(
      const std::optional<std::vector<media::VideoCaptureDeviceInfo>>&
          device_infos) override;
  void SetTitleTextAndTooltip(int message_id,
                              const std::vector<std::string>& device_names);
#endif

  // Adds Media (Camera or Mic) live preview feeds.
  void MaybeAddMediaPreview(content::WebContents* web_contents,
                            views::View& preceding_separator);

  raw_ptr<PageInfo> presenter_ = nullptr;
  ContentSettingsType type_;
  raw_ptr<ChromePageInfoUiDelegate> ui_delegate_ = nullptr;
  base::WeakPtr<content::WebContents> web_contents_;
  PageInfo::PermissionInfo permission_;

  raw_ptr<NonAccessibleImageView> icon_ = nullptr;
  raw_ptr<views::Label> title_ = nullptr;
  raw_ptr<views::Label> state_label_ = nullptr;
  raw_ptr<views::ToggleButton> toggle_button_ = nullptr;
  raw_ptr<views::Checkbox> remember_setting_ = nullptr;

#if !BUILDFLAG(IS_CHROMEOS)
  std::optional<PageInfoPreviewsCoordinator> previews_coordinator_;
  base::ScopedObservation<media_effects::MediaDeviceInfo,
                          PageInfoPermissionContentView>
      devices_observer_{this};
#endif
};

#endif  // CHROME_BROWSER_UI_VIEWS_PAGE_INFO_PAGE_INFO_PERMISSION_CONTENT_VIEW_H_
