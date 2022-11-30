// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_PUBLIC_CHROMECAST_EXPORT_H_
#define CHROMECAST_PUBLIC_CHROMECAST_EXPORT_H_

// Export attribute for classes that are exposed in shared libraries,
// allowing OEM partners to replace with their own implementations.
#define CHROMECAST_EXPORT __attribute__((visibility("default")))

#endif  // CHROMECAST_PUBLIC_CHROMECAST_EXPORT_H_
