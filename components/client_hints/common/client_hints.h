// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CLIENT_HINTS_COMMON_CLIENT_HINTS_H_
#define COMPONENTS_CLIENT_HINTS_COMMON_CLIENT_HINTS_H_

#include "components/content_settings/core/common/content_settings.h"

namespace blink {
class EnabledClientHints;
}

namespace client_hints {

const char kClientHintsSettingKey[] = "client_hints";

// The method updates |client_hints| with the result. |client_hints_cache|
// contains the content settings for the client hints.
void GetAllowedClientHints(const base::Value& client_hints_cache,
                           blink::EnabledClientHints* client_hints);

}  // namespace client_hints

#endif  // COMPONENTS_CLIENT_HINTS_COMMON_CLIENT_HINTS_H_
