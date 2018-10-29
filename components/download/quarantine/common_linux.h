// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DOWNLOAD_QUARANTINE_COMMON_LINUX_H_
#define COMPONENTS_DOWNLOAD_QUARANTINE_COMMON_LINUX_H_

namespace download {

// Attribute names to be used with setxattr and friends.
//
// The source URL attribute is part of the XDG standard.
// The referrer URL attribute is not part of the XDG standard,
// but it is used to keep the naming consistent.
// http://freedesktop.org/wiki/CommonExtendedAttributes
extern const char kSourceURLExtendedAttrName[];
extern const char kReferrerURLExtendedAttrName[];

}  // namespace download

#endif  // COMPONENTS_DOWNLOAD_QUARANTINE_COMMON_LINUX_H_
