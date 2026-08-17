// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ONE_TIME_TOKENS_CORE_COMMON_ONE_TIME_TOKEN_SWITCHES_H_
#define COMPONENTS_ONE_TIME_TOKENS_CORE_COMMON_ONE_TIME_TOKEN_SWITCHES_H_

#include "base/component_export.h"

namespace one_time_tokens::switches {

COMPONENT_EXPORT(ONE_TIME_TOKENS)
extern const char kOneTimeTokenServiceBaseUrl[];

COMPONENT_EXPORT(ONE_TIME_TOKENS)
extern const char kDefaultOneTimeTokenServiceBaseUrl[];

}  // namespace one_time_tokens::switches

#endif  // COMPONENTS_ONE_TIME_TOKENS_CORE_COMMON_ONE_TIME_TOKEN_SWITCHES_H_
