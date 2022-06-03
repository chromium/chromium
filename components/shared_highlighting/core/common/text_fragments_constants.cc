// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/shared_highlighting/core/common/text_fragments_constants.h"

namespace shared_highlighting {

const char kAnchorDelimiter = '#';

const char kFragmentsUrlDelimiter[] = ":~:";

const char kFragmentParameterName[] = "text=";

const char kFragmentPrefixKey[] = "prefix";
const char kFragmentTextStartKey[] = "textStart";
const char kFragmentTextEndKey[] = "textEnd";
const char kFragmentSuffixKey[] = "suffix";

// Light purple.
const int kFragmentTextBackgroundColorARGB = 0xFFE9D2FD;

// Black.
const int kFragmentTextForegroundColorARGB = 0xFF000000;

}  // namespace shared_highlighting
