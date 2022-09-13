// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEARCH_SEARCH_H_
#define COMPONENTS_SEARCH_SEARCH_H_

class TemplateURLService;

namespace search {

// Returns whether the Instant Extended API is enabled. This is always true on
// desktop and false on mobile.
bool IsInstantExtendedAPIEnabled();

// Returns whether Google is selected as the default search engine.
bool DefaultSearchProviderIsGoogle(
    const TemplateURLService* template_url_service);

}  // namespace search

#endif  // COMPONENTS_SEARCH_SEARCH_H_
