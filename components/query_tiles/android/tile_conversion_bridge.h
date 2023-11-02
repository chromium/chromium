// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_QUERY_TILES_ANDROID_TILE_CONVERSION_BRIDGE_H_
#define COMPONENTS_QUERY_TILES_ANDROID_TILE_CONVERSION_BRIDGE_H_

#include <vector>

#include "base/android/jni_android.h"
#include "components/query_tiles/tile.h"

using base::android::ScopedJavaLocalRef;

namespace query_tiles {

// Helper class providing tile conversion utility methods between C++ and Java.
class TileConversionBridge {
 public:
  static ScopedJavaLocalRef<jobject> CreateJavaTiles(
      JNIEnv* env,
      const std::vector<Tile>& tiles);

  static ScopedJavaLocalRef<jobject> CreateJavaTiles(
      JNIEnv* env,
      const std::vector<std::unique_ptr<Tile>>& tiles);
};

}  // namespace query_tiles

#endif  // COMPONENTS_QUERY_TILES_ANDROID_TILE_CONVERSION_BRIDGE_H_
