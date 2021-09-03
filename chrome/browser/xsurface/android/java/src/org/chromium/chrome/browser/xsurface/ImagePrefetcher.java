// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.xsurface;

// TODO(freedjm): Remove ImagePrefetcher after internal changes
//                land for ImageCacheHelper.

/**
 * Interface to prefetch an image and cache it on disk. This
 * allows native code to call to the image loader across the
 * xsurface.
 */
@Deprecated
public interface ImagePrefetcher extends ImageCacheHelper {}
