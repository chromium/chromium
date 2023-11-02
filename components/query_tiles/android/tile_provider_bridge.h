// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_QUERY_TILES_ANDROID_TILE_PROVIDER_BRIDGE_H_
#define COMPONENTS_QUERY_TILES_ANDROID_TILE_PROVIDER_BRIDGE_H_

#include "base/android/jni_android.h"
#include "base/memory/raw_ptr.h"
#include "base/supports_user_data.h"
#include "components/query_tiles/tile_service.h"

using base::android::JavaParamRef;
using base::android::JavaRef;
using base::android::ScopedJavaGlobalRef;
using base::android::ScopedJavaLocalRef;

namespace query_tiles {

// Helper class responsible for bridging the TileProvider between C++ and Java.
class TileProviderBridge : public base::SupportsUserData::Data {
 public:
  // Returns a Java TileProviderBridge for |tile_service|.  There will
  // be only one bridge per TileProviderBridge.
  static ScopedJavaLocalRef<jobject> GetBridgeForTileService(
      TileService* tile_service);

  explicit TileProviderBridge(TileService* tile_service);

  TileProviderBridge(const TileProviderBridge&) = delete;
  TileProviderBridge& operator=(const TileProviderBridge&) = delete;

  ~TileProviderBridge() override;

  // Methods called from Java via JNI.
  void GetQueryTiles(JNIEnv* env,
                     const JavaParamRef<jobject>& jcaller,
                     const JavaParamRef<jstring>& j_tile_id,
                     const JavaParamRef<jobject>& jcallback);

  // Called when a tile is clicked.
  void OnTileClicked(JNIEnv* env, const JavaParamRef<jstring>& j_tile_id);

 private:
  // A reference to the Java counterpart of this class.  See
  // TileProviderBridge.java.
  ScopedJavaGlobalRef<jobject> java_obj_;

  // Not owned.
  raw_ptr<TileService> tile_service_;
};

}  // namespace query_tiles

#endif  // COMPONENTS_QUERY_TILES_ANDROID_TILE_PROVIDER_BRIDGE_H_
