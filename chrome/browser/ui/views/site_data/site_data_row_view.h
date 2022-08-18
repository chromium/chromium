// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_SITE_DATA_SITE_DATA_ROW_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_SITE_DATA_SITE_DATA_ROW_VIEW_H_

#include "components/content_settings/core/common/content_settings.h"
#include "ui/views/view.h"

class FaviconCache;

namespace gfx {
class Image;
}  // namespace gfx

namespace views {
class ImageView;
class Label;
}  // namespace views

namespace url {
class Origin;
}  // namespace url

// The view that represents a site that has acesss to the data or was blocked
// from accessing the data in the context of the currently visited website. The
// view is used as a row in the site data dialog. It contains a favicon (with a
// fallback icon), a hostname and a menu icon. The menu allows to change the
// cookies content setting for the site or delete the site data.
class SiteDataRowView : public views::View {
 public:
  explicit SiteDataRowView(const url::Origin& origin,
                           ContentSetting setting,
                           FaviconCache* favicon_cache);

 private:
  void SetFaviconImage(const gfx::Image& image);

  void OnMenuIconClicked();

  void OnDeleteMenuItemClicked(int event_flags);
  void OnBlockMenuItemClicked(int event_flags);
  void OnAllowMenuItemClicked(int event_flags);
  void OnClearOnExitMenuItemClicked(int event_flags);

  // Sets a content setting exception for the |origin| with |setting| value.
  // Updates the UI to represent the new state: update the state label and the
  // content menu items. After an update the state label is always visible.
  void SetContentSettingException(ContentSetting setting);

  ContentSetting setting_;

  raw_ptr<views::Label> state_label_ = nullptr;
  raw_ptr<views::ImageView> favicon_image_ = nullptr;
};

#endif  // CHROME_BROWSER_UI_VIEWS_SITE_DATA_SITE_DATA_ROW_VIEW_H_
