// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/omnibox_client.h"

#include <memory>

#include "base/strings/string_util.h"
#include "ui/gfx/image/image.h"

std::unique_ptr<OmniboxNavigationObserver>
OmniboxClient::CreateOmniboxNavigationObserver(
    const base::string16& text,
    const AutocompleteMatch& match,
    const AutocompleteMatch& alternate_nav_match) {
  return nullptr;
}

bool OmniboxClient::CurrentPageExists() const {
  return true;
}

const GURL& OmniboxClient::GetURL() const {
  return GURL::EmptyGURL();
}

const base::string16& OmniboxClient::GetTitle() const {
  return base::EmptyString16();
}

gfx::Image OmniboxClient::GetFavicon() const {
  return gfx::Image();
}

bool OmniboxClient::IsLoading() const {
  return false;
}

bool OmniboxClient::IsPasteAndGoEnabled() const {
  return false;
}

bool OmniboxClient::IsDefaultSearchProviderEnabled() const {
  return true;
}

bookmarks::BookmarkModel* OmniboxClient::GetBookmarkModel() {
  return nullptr;
}

OmniboxControllerEmitter* OmniboxClient::GetOmniboxControllerEmitter() {
  return nullptr;
}

TemplateURLService* OmniboxClient::GetTemplateURLService() {
  return nullptr;
}

AutocompleteClassifier* OmniboxClient::GetAutocompleteClassifier() {
  return nullptr;
}

gfx::Image OmniboxClient::GetIconIfExtensionMatch(
    const AutocompleteMatch& match) const {
  return gfx::Image();
}

gfx::Image OmniboxClient::GetSizedIcon(const gfx::VectorIcon& vector_icon_type,
                                       SkColor vector_icon_color) const {
  return gfx::Image();
}

gfx::Image OmniboxClient::GetSizedIcon(const gfx::Image& icon) const {
  return gfx::Image();
}

bool OmniboxClient::ProcessExtensionKeyword(
    const TemplateURL* template_url,
    const AutocompleteMatch& match,
    WindowOpenDisposition disposition,
    OmniboxNavigationObserver* observer) {
  return false;
}

gfx::Image OmniboxClient::GetFaviconForPageUrl(
    const GURL& page_url,
    FaviconFetchedCallback on_favicon_fetched) {
  return gfx::Image();
}

gfx::Image OmniboxClient::GetFaviconForDefaultSearchProvider(
    FaviconFetchedCallback on_favicon_fetched) {
  return gfx::Image();
}

gfx::Image OmniboxClient::GetFaviconForKeywordSearchProvider(
    const TemplateURL* template_url,
    FaviconFetchedCallback on_favicon_fetched) {
  return gfx::Image();
}
