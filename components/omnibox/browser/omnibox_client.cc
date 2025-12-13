// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/omnibox_client.h"

#include <memory>

#include "base/strings/string_util.h"
#include "third_party/metrics_proto/omnibox_event.pb.h"
#include "ui/gfx/image/image.h"

bool OmniboxClient::CurrentPageExists() const {
  return true;
}

const GURL& OmniboxClient::GetURL() const {
  return GURL::EmptyGURL();
}

const std::u16string& OmniboxClient::GetTitle() const {
  return base::EmptyString16();
}

gfx::Image OmniboxClient::GetFavicon() const {
  return gfx::Image();
}

ukm::SourceId OmniboxClient::GetUKMSourceId() const {
  return ukm::kInvalidSourceId;
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

TemplateURLService* OmniboxClient::GetTemplateURLService() {
  return nullptr;
}

AutocompleteClassifier* OmniboxClient::GetAutocompleteClassifier() {
  return nullptr;
}

omnibox::OmniboxPopupCloser* OmniboxClient::GetOmniboxPopupCloser() {
  return nullptr;
}

bool OmniboxClient::ShouldDefaultTypedNavigationsToHttps() const {
  return false;
}

int OmniboxClient::GetHttpsPortForTesting() const {
  return 0;
}

metrics::OmniboxEventProto::PageClassification
OmniboxClient::GetOmniboxComposeboxPageClassification() const {
  return metrics::OmniboxEventProto::INVALID_SPEC;
}

bool OmniboxClient::IsUsingFakeHttpsForHttpsUpgradeTesting() const {
  return false;
}

gfx::Image OmniboxClient::GetExtensionIcon(
    const TemplateURL* template_url) const {
  return gfx::Image();
}

gfx::Image OmniboxClient::GetSizedIcon(const SkBitmap* bitmap) const {
  return gfx::Image();
}

gfx::Image OmniboxClient::GetSizedIcon(const gfx::VectorIcon& vector_icon_type,
                                       SkColor vector_icon_color) const {
  return gfx::Image();
}

gfx::Image OmniboxClient::GetSizedIcon(const gfx::Image& icon) const {
  return gfx::Image();
}

std::optional<lens::proto::LensOverlaySuggestInputs>
OmniboxClient::GetLensOverlaySuggestInputs() const {
  return std::nullopt;
}

std::optional<lens::ContextualInputData> OmniboxClient::GetContextualInputData()
    const {
  return std::nullopt;
}

void OmniboxClient::ProcessExtensionMatch(const std::u16string& text,
                                          const TemplateURL* template_url,
                                          const AutocompleteMatch& match,
                                          WindowOpenDisposition disposition) {}

void OmniboxClient::OnUserPastedInOmniboxResultingInValidURL() {}

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

bool OmniboxClient::IsHistoryEmbeddingsEnabled() const {
  return false;
}

bool OmniboxClient::IsAimPopupEnabled() const {
  return false;
}

omnibox::ChromeAimToolsAndModels OmniboxClient::AimToolMode() const {
  return omnibox::ChromeAimToolsAndModels::TOOL_MODE_UNSPECIFIED;
}
