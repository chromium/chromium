// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.query_tiles.bridges;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.base.Callback;
import org.chromium.components.query_tiles.QueryTile;
import org.chromium.components.query_tiles.TileProvider;

import java.util.List;

/**
 * Bridge to the native query tile service for the given {@link Profile}.
 */
@JNINamespace("query_tiles")
public class TileProviderBridge implements TileProvider {
    private long mNativeTileProviderBridge;

    @CalledByNative
    private static TileProviderBridge create(long nativePtr) {
        return new TileProviderBridge(nativePtr);
    }

    private TileProviderBridge(long nativePtr) {
        mNativeTileProviderBridge = nativePtr;
    }

    @CalledByNative
    private void clearNativePtr() {
        mNativeTileProviderBridge = 0;
    }

    @Override
    public void getQueryTiles(String tileId, Callback<List<QueryTile>> callback) {
        if (mNativeTileProviderBridge == 0) return;
        TileProviderBridgeJni.get().getQueryTiles(
                mNativeTileProviderBridge, this, tileId, callback);
    }

    @Override
    public void onTileClicked(String tildId) {
        if (mNativeTileProviderBridge == 0) return;
        TileProviderBridgeJni.get().onTileClicked(mNativeTileProviderBridge, tildId);
    }

    @NativeMethods
    interface Natives {
        void getQueryTiles(long nativeTileProviderBridge, TileProviderBridge caller, String tileId,
                Callback<List<QueryTile>> callback);
        void onTileClicked(long nativeTileProviderBridge, String tileId);
    }
}
