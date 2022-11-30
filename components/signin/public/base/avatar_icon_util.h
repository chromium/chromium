// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SIGNIN_PUBLIC_BASE_AVATAR_ICON_UTIL_H_
#define COMPONENTS_SIGNIN_PUBLIC_BASE_AVATAR_ICON_UTIL_H_

class GURL;

namespace signin {

// Size of |AccountInfo| image.
extern const int kAccountInfoImageSize;

// Given an image URL this function builds a new URL, appending passed
// |image_size| and |no_silhouette| parameters. |old_url| must be valid.
// For example, if |image_size| was set to 256, |no_silhouette| was set to
// true and |old_url| was either:
//   https://example.com/--Abc/AAAAAAAAAAI/AAAAAAAAACQ/Efg/photo.jpg
//   or
//   https://example.com/--Abc/AAAAAAAAAAI/AAAAAAAAACQ/Efg/s64-c-ns/photo.jpg
// then return value would be:
//   https://example.com/--Abc/AAAAAAAAAAI/AAAAAAAAACQ/Efg/s256-c-ns/photo.jpg
GURL GetAvatarImageURLWithOptions(const GURL& old_url,
                                  int image_size,
                                  bool no_silhouette);

}  // namespace signin

#endif  // COMPONENTS_SIGNIN_PUBLIC_BASE_AVATAR_ICON_UTIL_H_
