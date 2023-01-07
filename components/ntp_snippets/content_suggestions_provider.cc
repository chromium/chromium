// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ntp_snippets/content_suggestions_provider.h"

namespace ntp_snippets {

ContentSuggestionsProvider::ContentSuggestionsProvider(Observer* observer)
    : observer_(observer) {}

ContentSuggestionsProvider::~ContentSuggestionsProvider() = default;

}  // namespace ntp_snippets
