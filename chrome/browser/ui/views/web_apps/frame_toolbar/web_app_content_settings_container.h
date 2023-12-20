// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_WEB_APPS_FRAME_TOOLBAR_WEB_APP_CONTENT_SETTINGS_CONTAINER_H_
#define CHROME_BROWSER_UI_VIEWS_WEB_APPS_FRAME_TOOLBAR_WEB_APP_CONTENT_SETTINGS_CONTAINER_H_

#include <vector>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/views/location_bar/content_setting_image_view.h"
#include "chrome/browser/ui/views/location_bar/icon_label_bubble_view.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/view.h"

class WebAppContentSettingsContainer : public views::View {
  METADATA_HEADER(WebAppContentSettingsContainer, views::View)

 public:
  WebAppContentSettingsContainer(
      IconLabelBubbleView::Delegate* icon_label_bubble_delegate,
      ContentSettingImageView::Delegate* content_setting_image_delegate);
  WebAppContentSettingsContainer(const WebAppContentSettingsContainer&) =
      delete;
  WebAppContentSettingsContainer& operator=(
      const WebAppContentSettingsContainer&) = delete;
  ~WebAppContentSettingsContainer() override;

  void UpdateContentSettingViewsVisibility();

  // Sets the color of the content setting icons.
  void SetIconColor(SkColor icon_color);

  void SetUpForFadeIn();

  void FadeIn();

  void EnsureVisible();

  const std::vector<raw_ptr<ContentSettingImageView, VectorExperimental>>&
  get_content_setting_views() const {
    return content_setting_views_;
  }

 private:
  // Owned by the views hierarchy.
  std::vector<raw_ptr<ContentSettingImageView, VectorExperimental>>
      content_setting_views_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_WEB_APPS_FRAME_TOOLBAR_WEB_APP_CONTENT_SETTINGS_CONTAINER_H_
