// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_GROUPS_AVATAR_CONTAINER_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_GROUPS_AVATAR_CONTAINER_VIEW_H_

#include "components/data_sharing/public/group_data.h"
#include "components/saved_tab_groups/public/types.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/view.h"

class Profile;

namespace data_sharing {
class DataSharingService;
}

// A face pile of the avatars in a shared tab group, used for the tab group
// editor bubble view in the ManageSharingRow
class ManageSharingAvatarContainer : public views::View {
  METADATA_HEADER(ManageSharingAvatarContainer, views::View)
 public:
  ManageSharingAvatarContainer(
      Profile* profile,
      const tab_groups::CollaborationId& collaboration_id);
  ~ManageSharingAvatarContainer() override;

  // destroys and rebuilds all member images, used for retheming. does not
  // requery.
  void RebuildChildren();

  // views::View overrides.
  void AddedToWidget() override;
  void OnThemeChanged() override;

 protected:
  void OnDeviceScaleFactorChanged(float old_device_scale_factor,
                                  float new_device_scale_factor) override;

 private:
  // Callback when data sharing service fetches the avatar.
  void UpdateMemberGfxImage(size_t index, const gfx::Image&);

  // function that clears out member info and gets it again.
  void RequeryMemberInfo();

  // The service that avatar images are being pulled from.
  raw_ptr<data_sharing::DataSharingService> data_sharing_service_;

  // The profile for pulling group information.
  raw_ptr<Profile> const profile_;

  // The saved GUID for the group (used to get the collaboration).
  const tab_groups::CollaborationId collaboration_id_;

  // the members list that were queried from the data_sharing_service.
  std::vector<data_sharing::GroupMember> members_for_display_;

  // images for each of the members displayed.
  std::array<std::optional<gfx::Image>, 3> member_gfx_images_;

  // ImageViews displayed for each of the members.
  raw_ptr<views::ImageView> member_1_image_view_ = nullptr;
  raw_ptr<views::ImageView> member_2_image_view_ = nullptr;
  raw_ptr<views::ImageView> member_3_or_overflow_image_view_ = nullptr;

  base::WeakPtrFactory<ManageSharingAvatarContainer> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_GROUPS_AVATAR_CONTAINER_VIEW_H_
