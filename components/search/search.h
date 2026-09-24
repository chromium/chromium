// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEARCH_SEARCH_H_
#define COMPONENTS_SEARCH_SEARCH_H_

class TemplateURLService;
class TemplateURL;
class SearchTermsData;

namespace search {

// Returns whether the Instant Extended API is enabled. This is always true on
// desktop and false on mobile.
bool IsInstantExtendedAPIEnabled();

// Returns whether Google is selected as the default search engine.
bool DefaultSearchProviderIsGoogle(
    const TemplateURLService* template_url_service);

// Returns true if `template_url` is non-nullptr and its search URL is
// cryptographic (or loopback/localhost) and resolves to
// `SEARCH_ENGINE_GOOGLE`. If `template_url` specifies a suggestions URL, that
// URL must also be cryptographic (or loopback/localhost) and resolve to
// `SEARCH_ENGINE_GOOGLE`.
bool TemplateURLIsGoogle(const TemplateURL* template_url,
                         const SearchTermsData& search_terms_data);
}  // namespace search

#endif  // COMPONENTS_SEARCH_SEARCH_H_
