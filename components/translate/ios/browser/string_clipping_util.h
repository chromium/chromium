// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_TRANSLATE_IOS_BROWSER_STRING_CLIPPING_UTIL_H_
#define COMPONENTS_TRANSLATE_IOS_BROWSER_STRING_CLIPPING_UTIL_H_

#include <stddef.h>

#include <string>

#include "base/strings/string16.h"

// Truncates |contents| to |length|.
// Returns a string terminated at the last space to ensure no words are
// clipped.
// Note: This function uses spaces as word boundaries and may not handle all
// languages correctly.
base::string16 GetStringByClippingLastWord(const base::string16& contents,
                                           size_t length);

#endif  // COMPONENTS_TRANSLATE_IOS_BROWSER_STRING_CLIPPING_UTIL_H_
