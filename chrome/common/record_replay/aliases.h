// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_RECORD_REPLAY_ALIASES_H_
#define CHROME_COMMON_RECORD_REPLAY_ALIASES_H_

#include <string>

#include "base/types/strong_alias.h"

namespace record_replay {

// Uniquely identifies a DOM element in a document.
using DomNodeId = base::StrongAlias<struct DomNodeIdTag, int64_t>;

// The value of a form control element or contenteditable.
//
// Since this is untrusted data from a renderer, it must not be interpreted
// in the browser process.
using FieldValue = base::StrongAlias<struct FieldValueTag, std::string>;

// Identify a DOM element. Unlike DomNodeId, a selector is not guaranteed to
// be unique but attempts to be stable across page loads.
//
// Since this is untrusted data from a renderer, it must not be interpreted
// in the browser process.
using Selector = base::StrongAlias<struct SelectorTag, std::string>;

}  // namespace record_replay

#endif  // CHROME_COMMON_RECORD_REPLAY_ALIASES_H_
