// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_MEDIA_GALLERIES_METADATA_TYPES_H_
#define CHROME_COMMON_MEDIA_GALLERIES_METADATA_TYPES_H_

#include <string>

namespace metadata {

struct AttachedImage {
  std::string type;
  std::string data;
};

}  // namespace metadata

#endif  // CHROME_COMMON_MEDIA_GALLERIES_METADATA_TYPES_H_
