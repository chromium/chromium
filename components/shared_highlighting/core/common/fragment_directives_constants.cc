// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/shared_highlighting/core/common/fragment_directives_constants.h"

namespace shared_highlighting {

const char kAnchorDelimiter = '#';

const char kFragmentsUrlDelimiter[] = ":~:";
const int kFragmentsUrlDelimiterLength = strlen(kFragmentsUrlDelimiter);

const char kTextDirectiveParameterName[] = "text=";
const size_t kTextDirectiveParameterNameLength =
    strlen(kTextDirectiveParameterName);

const char kSelectorJoinDelimeter[] = "&";
const int kSelectorJoinDelimeterLength = strlen(kSelectorJoinDelimeter);

const char kFragmentPrefixKey[] = "prefix";
const char kFragmentTextStartKey[] = "textStart";
const char kFragmentTextEndKey[] = "textEnd";
const char kFragmentSuffixKey[] = "suffix";

// Light purple.
const int kFragmentTextBackgroundColorARGB = 0xFFE9D2FD;

// Black.
const int kFragmentTextForegroundColorARGB = 0xFF000000;

}  // namespace shared_highlighting
