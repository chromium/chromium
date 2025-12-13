// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_GROUPS_MANAGE_SHARING_ROW_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_GROUPS_MANAGE_SHARING_ROW_H_

#include "base/uuid.h"
#include "components/sync/base/collaboration_id.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/controls/button/button.h"

class Profile;
class ManageSharingAvatarContainer;

namespace views {

class ImageView;
class InkDropContainerView;
class Label;
class LabelButton;

}  // namespace views

class ManageSharingRow : public views::Button {
  METADATA_HEADER(ManageSharingRow, views::Button)

 public:
  ManageSharingRow(Profile* profile,
                   const syncer::CollaborationId& collaboration_id_,
                   PressedCallback callback);
  void RebuildChildren();
  ~ManageSharingRow() override;
  void StateChanged(ButtonState old_state) override;
  void UpdateInsetsToMatchLabelButton(views::LabelButton* button);
  void AddedToWidget() override;
  void OnThemeChanged() override;
  void OnFocus() override;
  void OnBlur() override;
  void UpdateBackgroundColor();

  // Button:
  void AddLayerToRegion(ui::Layer* new_layer,
                        views::LayerRegion region) override;
  void RemoveLayerFromRegions(ui::Layer* old_layer) override;

 private:
  // Member variables for UI components.
  raw_ptr<views::InkDropContainerView> ink_drop_container_ = nullptr;
  raw_ptr<views::ImageView> manage_group_icon_ = nullptr;
  raw_ptr<views::Label> manage_group_label_ = nullptr;
  raw_ptr<ManageSharingAvatarContainer> avatar_container_ = nullptr;

  raw_ptr<Profile> profile_ = nullptr;
  syncer::CollaborationId collaboration_id_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_GROUPS_MANAGE_SHARING_ROW_H_
