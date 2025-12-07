// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PAGE_INFO_PAGE_INFO_BUBBLE_SPECIFICATION_H_
#define CHROME_BROWSER_UI_VIEWS_PAGE_INFO_PAGE_INFO_BUBBLE_SPECIFICATION_H_

#include <memory>
#include <optional>

#include "base/memory/raw_ptr.h"
#include "base/types/pass_key.h"
#include "chrome/browser/ui/page_info/page_info_dialog.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "ui/gfx/native_ui_types.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"

namespace gfx {
class Rect;
}

namespace content {
class WebContents;
}

class GURL;

class PageInfoBubbleSpecification {
 public:
  class Builder final {
   public:
    Builder(views::BubbleAnchor anchor,
            gfx::NativeWindow parent_window,
            content::WebContents* web_contents,
            const GURL& url);
    ~Builder();
    Builder(const Builder& other) = delete;
    Builder& operator=(const Builder& other) = delete;

    // Anchor rect will be used to anchor the bubble if `anchor` is null.
    Builder& AddAnchorRect(gfx::Rect rect);

    // `callback` will run after the page info UI initializes and is presented
    // in the bubble UI.
    Builder& AddInitializedCallback(base::OnceClosure callback);

    // `callback` will run when the bubble widget is destroying.
    Builder& AddPageInfoClosingCallback(PageInfoClosingCallback callback);

    // Hides the extended site info section at the bottom of the page info
    // bubble.
    Builder& HideExtendedSiteInfo();

    // Opens a page in the page info bubble. Note: only one page can be shown at
    // a time, so a permission page is mutually exclusive with the merchant
    // trust page.
    Builder& ShowPermissionPage(ContentSettingsType type);
    Builder& ShowMerchantTrustPage();

    std::unique_ptr<PageInfoBubbleSpecification> Build();

   private:
    void ValidateSpecification();

    std::unique_ptr<PageInfoBubbleSpecification>
        page_info_bubble_specification_;
  };

  PageInfoBubbleSpecification(base::PassKey<Builder>,
                              views::BubbleAnchor anchor,
                              gfx::NativeWindow parent_window,
                              content::WebContents* web_contents,
                              const GURL& url);
  ~PageInfoBubbleSpecification();

  void AddAnchorRect(gfx::Rect rect);
  void AddInitializedCallback(base::OnceClosure callback);
  void AddPageInfoClosingCallback(PageInfoClosingCallback callback);
  void HideExtendedSiteInfo();
  void ShowPermissionPage(ContentSettingsType type);
  void ShowMerchantTrustPage();

  views::BubbleAnchor anchor();
  gfx::NativeWindow parent_window();
  content::WebContents* web_contents();
  const GURL& url();
  gfx::Rect anchor_rect();
  base::OnceClosure initialized_callback();
  PageInfoClosingCallback page_info_closing_callback();
  bool show_extended_site_info();
  std::optional<ContentSettingsType> permission_page_type();
  bool show_merchant_trust_page();

 private:
  views::BubbleAnchor anchor_;
  gfx::NativeWindow parent_window_;
  raw_ptr<content::WebContents> web_contents_;
  GURL url_;
  gfx::Rect anchor_rect_;
  base::OnceClosure initialized_callback_;
  PageInfoClosingCallback page_info_closing_callback_;
  bool show_extended_site_info_ = true;
  std::optional<ContentSettingsType> permission_page_type_;
  bool show_merchant_trust_page_ = false;
};

#endif  // CHROME_BROWSER_UI_VIEWS_PAGE_INFO_PAGE_INFO_BUBBLE_SPECIFICATION_H_
