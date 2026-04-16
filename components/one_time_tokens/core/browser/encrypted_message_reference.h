// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ONE_TIME_TOKENS_CORE_BROWSER_ENCRYPTED_MESSAGE_REFERENCE_H_
#define COMPONENTS_ONE_TIME_TOKENS_CORE_BROWSER_ENCRYPTED_MESSAGE_REFERENCE_H_

#include <string>

#include "base/types/strong_alias.h"

namespace one_time_tokens {

using EncryptedMessageReference =
    base::StrongAlias<class EncryptedMessageReferenceTag, std::string>;

}  // namespace one_time_tokens

#endif  // COMPONENTS_ONE_TIME_TOKENS_CORE_BROWSER_ENCRYPTED_MESSAGE_REFERENCE_H_
