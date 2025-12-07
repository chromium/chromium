// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/mahi/public/cpp/mahi_browser_util.h"

#include <string>

#include "url/gurl.h"

namespace mahi {

WebContentState::WebContentState(const WebContentState& state) = default;

WebContentState::WebContentState(const GURL& url, const std::u16string& title)
    : url(url), title(title) {}

WebContentState::~WebContentState() = default;

}  // namespace mahi
