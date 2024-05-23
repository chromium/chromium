// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/query_tiles/android/tile_conversion_bridge.h"

#include <memory>
#include <string>

#include "base/android/jni_string.h"
#include "url/android/gurl_android.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "components/query_tiles/jni_headers/TileConversionBridge_jni.h"

namespace query_tiles {

ScopedJavaLocalRef<jobject> CreateJavaTileAndMaybeAddToList(
    JNIEnv* env,
    ScopedJavaLocalRef<jobject> jlist,
    const Tile& tile) {
  ScopedJavaLocalRef<jobject> jchildren =
      Java_TileConversionBridge_createList(env);

  for (const auto& subtile : tile.sub_tiles)
    CreateJavaTileAndMaybeAddToList(env, jchildren, *subtile.get());

  std::vector<const GURL*> urls;
  for (const ImageMetadata& image : tile.image_metadatas)
    urls.push_back(&image.url);

  return Java_TileConversionBridge_createTileAndMaybeAddToList(
      env, jlist, tile.id, tile.display_text, tile.accessibility_text,
      tile.query_text, urls, tile.search_params, jchildren);
}

ScopedJavaLocalRef<jobject> TileConversionBridge::CreateJavaTiles(
    JNIEnv* env,
    const std::vector<Tile>& tiles) {
  ScopedJavaLocalRef<jobject> jlist = Java_TileConversionBridge_createList(env);

  for (const auto& tile : tiles)
    CreateJavaTileAndMaybeAddToList(env, jlist, tile);

  return jlist;
}

ScopedJavaLocalRef<jobject> TileConversionBridge::CreateJavaTiles(
    JNIEnv* env,
    const std::vector<std::unique_ptr<Tile>>& tiles) {
  ScopedJavaLocalRef<jobject> jlist = Java_TileConversionBridge_createList(env);

  for (const auto& tile : tiles)
    CreateJavaTileAndMaybeAddToList(env, jlist, *tile);

  return jlist;
}

}  // namespace query_tiles
