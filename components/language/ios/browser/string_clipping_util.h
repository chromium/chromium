// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_LANGUAGE_IOS_BROWSER_STRING_CLIPPING_UTIL_H_
#define COMPONENTS_LANGUAGE_IOS_BROWSER_STRING_CLIPPING_UTIL_H_

#include <stddef.h>

#include <string>

// Truncates |contents| to |length|.
// Returns a string terminated at the last space to ensure no words are
// clipped.
// Note: This function uses spaces as word boundaries and may not handle all
// languages correctly.
std::u16string GetStringByClippingLastWord(const std::u16string& contents,
                                           size_t length);

#endif  // COMPONENTS_LANGUAGE_IOS_BROWSER_STRING_CLIPPING_UTIL_H_
