// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_CONTENT_SETTING_SITE_ROW_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_CONTENT_SETTING_SITE_ROW_VIEW_H_

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/task/cancelable_task_tracker.h"
#include "net/base/schemeful_site.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/controls/button/toggle_button.h"
#include "ui/views/view.h"

namespace views {
class ToggleButton;
class ImageView;
}  // namespace views

namespace favicon {
class FaviconService;
}

namespace favicon_base {
struct FaviconRawBitmapResult;
}

// View with the name of a site and a toggle to change the permission of that
// site.
class ContentSettingSiteRowView : public views::View {
  METADATA_HEADER(ContentSettingSiteRowView, views::View)

 public:
  using ToggleCallback =
      base::RepeatingCallback<void(const net::SchemefulSite& site,
                                   bool allowed)>;

  ContentSettingSiteRowView(favicon::FaviconService* favicon_service,
                            const net::SchemefulSite& site,
                            bool allowed,
                            ToggleCallback toggle_callback);
  ~ContentSettingSiteRowView() override;
  ContentSettingSiteRowView(const ContentSettingSiteRowView&) = delete;
  ContentSettingSiteRowView& operator=(const ContentSettingSiteRowView&) =
      delete;

  views::ToggleButton* GetToggleForTesting() { return toggle_button_; }

 private:
  void OnToggleButtonPressed();
  void OnFaviconLoaded(
      const favicon_base::FaviconRawBitmapResult& favicon_result);

  net::SchemefulSite site_;
  ToggleCallback toggle_callback_;
  raw_ptr<views::ToggleButton> toggle_button_;
  raw_ptr<views::ImageView> favicon_;

  base::CancelableTaskTracker favicon_tracker_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_CONTENT_SETTING_SITE_ROW_VIEW_H_
