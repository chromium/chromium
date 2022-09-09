// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_CONTENT_RESTRICTION_H_
#define CHROME_COMMON_CONTENT_RESTRICTION_H_

// Used by a full-page plugin to disable browser commands because of
// restrictions on how the data is to be used (i.e. can't copy/print).
enum ContentRestriction {
  CONTENT_RESTRICTION_COPY  = 1 << 0,
  CONTENT_RESTRICTION_CUT   = 1 << 1,
  CONTENT_RESTRICTION_PASTE = 1 << 2,
  CONTENT_RESTRICTION_PRINT = 1 << 3,
  CONTENT_RESTRICTION_SAVE  = 1 << 4
};

#endif  // CHROME_COMMON_CONTENT_RESTRICTION_H_
