// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NTP_SNIPPETS_CALLBACKS_H_
#define COMPONENTS_NTP_SNIPPETS_CALLBACKS_H_

#include <string>
#include <vector>

#include "base/functional/callback_forward.h"

namespace gfx {
class Image;
}  // namespace gfx

namespace ntp_snippets {

class ContentSuggestion;

struct Status;

// Returns the result of a |Fetch| call by a ContentSuggestionsProvider.
using FetchDoneCallback =
    base::OnceCallback<void(Status status_code,
                            std::vector<ContentSuggestion> suggestions)>;

// Returns the resulting image of a |FetchSuggestionImage| call by a
// ContentSuggestionsProvider.
using ImageFetchedCallback = base::OnceCallback<void(const gfx::Image&)>;

using ImageDataFetchedCallback =
    base::OnceCallback<void(const std::string& image_data)>;

// Returns the list of dismissed suggestions when invoked. Currently only used
// for debugging methods to check the internal state of a provider.
using DismissedSuggestionsCallback = base::OnceCallback<void(
    std::vector<ContentSuggestion> dismissed_suggestions)>;

}  // namespace ntp_snippets

#endif  // COMPONENTS_NTP_SNIPPETS_CALLBACKS_H_
