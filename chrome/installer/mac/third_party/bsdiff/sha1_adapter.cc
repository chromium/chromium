// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/mac/third_party/bsdiff/sha1_adapter.h"

#include "base/hash/sha1.h"

void SHA1(const unsigned char* data, size_t len, unsigned char* hash) {
  base::SHA1HashBytes(data, len, hash);
}
