// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/query_tiles/android/tile_provider_bridge.h"

#include <memory>
#include <string>
#include <vector>

#include "base/android/callback_android.h"
#include "base/android/jni_string.h"
#include "base/functional/bind.h"
#include "components/query_tiles/android/tile_conversion_bridge.h"
#include "components/query_tiles/jni_headers/TileProviderBridge_jni.h"
#include "ui/gfx/android/java_bitmap.h"
#include "ui/gfx/image/image.h"

using base::android::AttachCurrentThread;

namespace query_tiles {

namespace {
const char kTileProviderBridgeKey[] = "tile_provider_bridge";

void RunGetTilesCallback(const JavaRef<jobject>& j_callback,
                         std::vector<Tile> tiles) {
  JNIEnv* env = AttachCurrentThread();
  RunObjectCallbackAndroid(
      j_callback, TileConversionBridge::CreateJavaTiles(env, std::move(tiles)));
}

void RunGetTileCallback(const JavaRef<jobject>& j_callback,
                        absl::optional<Tile> tile) {
  JNIEnv* env = AttachCurrentThread();
  RunObjectCallbackAndroid(
      j_callback,
      TileConversionBridge::CreateJavaTiles(
          env, tile.has_value() ? std::move(tile->sub_tiles)
                                : std::vector<std::unique_ptr<Tile>>()));
}

}  // namespace

// static
ScopedJavaLocalRef<jobject> TileProviderBridge::GetBridgeForTileService(
    TileService* tile_service) {
  if (!tile_service->GetUserData(kTileProviderBridgeKey)) {
    tile_service->SetUserData(
        kTileProviderBridgeKey,
        std::make_unique<TileProviderBridge>(tile_service));
  }

  TileProviderBridge* bridge = static_cast<TileProviderBridge*>(
      tile_service->GetUserData(kTileProviderBridgeKey));

  return ScopedJavaLocalRef<jobject>(bridge->java_obj_);
}

TileProviderBridge::TileProviderBridge(TileService* tile_service)
    : tile_service_(tile_service) {
  DCHECK(tile_service_);
  JNIEnv* env = base::android::AttachCurrentThread();
  java_obj_.Reset(
      env, Java_TileProviderBridge_create(env, reinterpret_cast<int64_t>(this))
               .obj());
}

TileProviderBridge::~TileProviderBridge() {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_TileProviderBridge_clearNativePtr(env, java_obj_);
}

void TileProviderBridge::GetQueryTiles(JNIEnv* env,
                                       const JavaParamRef<jobject>& jcaller,
                                       const JavaParamRef<jstring>& j_tile_id,
                                       const JavaParamRef<jobject>& jcallback) {
  // TODO(qinmin): refactor TileService to use a single call to handle both
  // cases.
  if (j_tile_id.is_null()) {
    tile_service_->GetQueryTiles(base::BindOnce(
        &RunGetTilesCallback, ScopedJavaGlobalRef<jobject>(jcallback)));
  } else {
    tile_service_->GetTile(
        ConvertJavaStringToUTF8(env, j_tile_id),
        base::BindOnce(&RunGetTileCallback,
                       ScopedJavaGlobalRef<jobject>(jcallback)));
  }
}

void TileProviderBridge::OnTileClicked(JNIEnv* env,
                                       const JavaParamRef<jstring>& j_tile_id) {
  tile_service_->OnTileClicked(ConvertJavaStringToUTF8(env, j_tile_id));
}

}  // namespace query_tiles
