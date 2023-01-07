// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ntp_snippets/remote/remote_suggestions_provider.h"

namespace ntp_snippets {

RemoteSuggestionsProvider::RemoteSuggestionsProvider(Observer* observer)
    : ContentSuggestionsProvider(observer) {}

RemoteSuggestionsProvider::~RemoteSuggestionsProvider() = default;

}  // namespace ntp_snippets
