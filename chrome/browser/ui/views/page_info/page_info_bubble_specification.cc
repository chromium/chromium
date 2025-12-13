// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/page_info/page_info_bubble_specification.h"

#include <algorithm>
#include <memory>

#include "base/check.h"
#include "base/functional/callback_helpers.h"
#include "chrome/browser/ui/page_info/page_info_dialog.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "ui/views/view.h"

PageInfoBubbleSpecification::Builder::Builder(
    views::BubbleAnchor anchor,
    gfx::NativeWindow parent_window,
    content::WebContents* web_contents,
    const GURL& url)
    : page_info_bubble_specification_(
          std::make_unique<PageInfoBubbleSpecification>(
              base::PassKey<Builder>{},
              anchor,
              parent_window,
              web_contents,
              url)) {}

PageInfoBubbleSpecification::Builder::~Builder() {
  // Verify that PageInfoBubbleSpecification::Builder::Build() has been called
  // so the page info bubble specification is completely built.
  CHECK(!page_info_bubble_specification_);
}

PageInfoBubbleSpecification::Builder&
PageInfoBubbleSpecification::Builder::AddAnchorRect(gfx::Rect rect) {
  page_info_bubble_specification_->AddAnchorRect(rect);
  return *this;
}

PageInfoBubbleSpecification::Builder&
PageInfoBubbleSpecification::Builder::AddInitializedCallback(
    base::OnceClosure callback) {
  page_info_bubble_specification_->AddInitializedCallback(std::move(callback));
  return *this;
}

PageInfoBubbleSpecification::Builder&
PageInfoBubbleSpecification::Builder::AddPageInfoClosingCallback(
    PageInfoClosingCallback callback) {
  page_info_bubble_specification_->AddPageInfoClosingCallback(
      std::move(callback));
  return *this;
}

PageInfoBubbleSpecification::Builder&
PageInfoBubbleSpecification::Builder::HideExtendedSiteInfo() {
  page_info_bubble_specification_->HideExtendedSiteInfo();
  return *this;
}

PageInfoBubbleSpecification::Builder&
PageInfoBubbleSpecification::Builder::ShowPermissionPage(
    ContentSettingsType type) {
  page_info_bubble_specification_->ShowPermissionPage(type);
  return *this;
}

PageInfoBubbleSpecification::Builder&
PageInfoBubbleSpecification::Builder::ShowMerchantTrustPage() {
  page_info_bubble_specification_->ShowMerchantTrustPage();
  return *this;
}

void PageInfoBubbleSpecification::Builder::ValidateSpecification() {
  CHECK(page_info_bubble_specification_->web_contents());

  // Bubbles on creation can show either the content settings page or the
  // merchant trust page but not both.
  if (page_info_bubble_specification_->permission_page_type().has_value()) {
    CHECK(!page_info_bubble_specification_->show_merchant_trust_page());
  }
}

std::unique_ptr<PageInfoBubbleSpecification>
PageInfoBubbleSpecification::Builder::Build() {
  ValidateSpecification();
  return std::move(page_info_bubble_specification_);
}

PageInfoBubbleSpecification::PageInfoBubbleSpecification(
    base::PassKey<Builder>,
    views::BubbleAnchor anchor,
    gfx::NativeWindow parent_window,
    content::WebContents* web_contents,
    const GURL& url)
    : anchor_(anchor),
      parent_window_(parent_window),
      web_contents_(web_contents),
      url_(url) {}

PageInfoBubbleSpecification::~PageInfoBubbleSpecification() = default;

void PageInfoBubbleSpecification::AddAnchorRect(gfx::Rect rect) {
  anchor_rect_ = rect;
}

void PageInfoBubbleSpecification::AddInitializedCallback(
    base::OnceClosure callback) {
  initialized_callback_ = std::move(callback);
}

void PageInfoBubbleSpecification::AddPageInfoClosingCallback(
    PageInfoClosingCallback callback) {
  page_info_closing_callback_ = std::move(callback);
}

void PageInfoBubbleSpecification::HideExtendedSiteInfo() {
  show_extended_site_info_ = false;
}

void PageInfoBubbleSpecification::ShowPermissionPage(ContentSettingsType type) {
  permission_page_type_ = type;
}

void PageInfoBubbleSpecification::ShowMerchantTrustPage() {
  show_merchant_trust_page_ = true;
}

views::BubbleAnchor PageInfoBubbleSpecification::anchor() {
  return anchor_;
}

gfx::NativeWindow PageInfoBubbleSpecification::parent_window() {
  return parent_window_;
}

content::WebContents* PageInfoBubbleSpecification::web_contents() {
  return web_contents_;
}

const GURL& PageInfoBubbleSpecification::url() {
  return url_;
}

gfx::Rect PageInfoBubbleSpecification::anchor_rect() {
  return anchor_rect_;
}

base::OnceClosure PageInfoBubbleSpecification::initialized_callback() {
  return initialized_callback_.is_null() ? base::DoNothing()
                                         : std::move(initialized_callback_);
}

PageInfoClosingCallback
PageInfoBubbleSpecification::page_info_closing_callback() {
  return page_info_closing_callback_.is_null()
             ? base::DoNothing()
             : std::move(page_info_closing_callback_);
}

bool PageInfoBubbleSpecification::show_extended_site_info() {
  return show_extended_site_info_;
}

std::optional<ContentSettingsType>
PageInfoBubbleSpecification::permission_page_type() {
  return permission_page_type_;
}

bool PageInfoBubbleSpecification::show_merchant_trust_page() {
  return show_merchant_trust_page_;
}
