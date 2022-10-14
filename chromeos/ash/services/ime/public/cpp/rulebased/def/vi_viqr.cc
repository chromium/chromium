// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/ime/public/cpp/rulebased/def/vi_viqr.h"

#include <iterator>

namespace vi_viqr {

const char* kId = "vi_viqr";
bool kIs102 = false;
const char* kTransforms[] = {
    "dd",      "\u0111",  "D[dD]",   "\u0110",  "a\\(",    "\u0103",  "a\\^",
    "\u00e2",  "e\\^",    "\u00ea",  "o\\^",    "\u00f4",  "o\\+",    "\u01a1",
    "u\\+",    "\u01b0",  "A\\(",    "\u0102",  "A\\^",    "\u00c2",  "E\\^",
    "\u00ca",  "O\\^",    "\u00d4",  "O\\+",    "\u01a0",  "U\\+",    "\u01af",
    "\\\\\\(", "(",       "\\\\\\^", "^",       "\\\\\\+", "+",       "\\\\\\`",
    "`",       "\\\\\\'", "'",       "\\\\\\?", "?",       "\\\\\\~", "~",
    "\\\\\\.", ".",       "\\`",     "\u0300",  "\\'",     "\u0301",  "\\?",
    "\u0309",  "\\~",     "\u0303",  "\\.",     "\u0323"};
const unsigned int kTransformsLen = std::size(kTransforms);
const char* kHistoryPrune = nullptr;

}  // namespace vi_viqr
