// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_SITE_DATA_SITE_DATA_ROW_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_SITE_DATA_SITE_DATA_ROW_VIEW_H_

#include "components/content_settings/core/common/content_settings.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/view.h"
#include "url/origin.h"

class FaviconCache;
class Profile;

namespace gfx {
class Image;
}  // namespace gfx

namespace views {
class ImageView;
class Label;
class ImageButton;
class MenuRunner;
}  // namespace views

namespace ui {
class MenuModel;
}  // namespace ui

namespace url {
class Origin;
}  // namespace url

DECLARE_CUSTOM_ELEMENT_EVENT_TYPE(kSiteRowMenuItemClicked);

// The view that represents a site that has acesss to the data or was blocked
// from accessing the data in the context of the currently visited website. The
// view is used as a row in the site data dialog. It contains a favicon (with a
// fallback icon), a hostname and a menu icon. The menu allows to change the
// cookies content setting for the site or delete the site data.
class SiteDataRowView : public views::View {
  METADATA_HEADER(SiteDataRowView, views::View)

 public:
  SiteDataRowView(
      Profile* profile,
      const url::Origin& origin,
      ContentSetting setting,
      bool is_fully_partitioned,
      FaviconCache* favicon_cache,
      base::RepeatingCallback<void(const url::Origin&)> delete_callback,
      base::RepeatingCallback<void(const url::Origin&, ContentSetting)>
          create_exception_callback);

  ~SiteDataRowView() override;

  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kMenuButton);
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kDeleteButton);

  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kAllowMenuItem);
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kBlockMenuItem);
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kClearOnExitMenuItem);

  views::Label* hostname_label_for_testing() { return hostname_label_; }
  views::Label* state_label_for_testing() { return state_label_; }
  views::ImageButton* menu_button_for_testing() { return menu_button_; }
  views::ImageButton* delete_button_for_testing() { return delete_button_; }

 private:
  friend class PageSpecificSiteDataDialogBrowserTest;

  void SetFaviconImage(const gfx::Image& image);

  void OnMenuIconClicked();
  void OnMenuClosed();

  void OnDeleteIconClicked();

  void OnBlockMenuItemClicked(int event_flags);
  void OnAllowMenuItemClicked(int event_flags);
  void OnClearOnExitMenuItemClicked(int event_flags);

  // Sets a content setting exception for the |origin| with |setting| value.
  // Updates the UI to represent the new state: update the state label and the
  // content menu items. After an update the state label is always visible.
  void SetContentSettingException(ContentSetting setting);

  url::Origin origin_;
  ContentSetting setting_;
  bool is_fully_partitioned_;
  base::RepeatingCallback<void(const url::Origin&)> delete_callback_;
  base::RepeatingCallback<void(const url::Origin&, ContentSetting)>
      create_exception_callback_;

  raw_ptr<views::Label> hostname_label_ = nullptr;
  raw_ptr<views::Label> state_label_ = nullptr;
  raw_ptr<views::ImageView> favicon_image_ = nullptr;
  raw_ptr<views::ImageButton> menu_button_ = nullptr;
  raw_ptr<views::ImageButton> delete_button_ = nullptr;

  std::unique_ptr<ui::MenuModel> dialog_model_;
  std::unique_ptr<views::MenuRunner> menu_runner_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_SITE_DATA_SITE_DATA_ROW_VIEW_H_
