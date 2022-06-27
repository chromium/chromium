// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.offline_items_collection;

import android.os.Handler;

import org.chromium.base.Callback;
import org.chromium.base.ObserverList;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;

import java.util.ArrayList;

/**
 * A helper class responsible for exposing the C++ OfflineContentAggregator
 * (components/offline_items_collection/core/offline_content_aggregator.h) class to Java.  This
 * class is created and owned by it's C++ counterpart OfflineContentAggregatorBridge
 * (components/offline_items_collection/core/android/offline_content_aggregator_bridge.h).
 */
@JNINamespace("offline_items_collection::android")
public class OfflineContentAggregatorBridge implements OfflineContentProvider {
    private final Handler mHandler = new Handler();

    private long mNativeOfflineContentAggregatorBridge;
    private ObserverList<OfflineContentProvider.Observer> mObservers;

    /**
     * A private constructor meant to be called by the C++ OfflineContentAggregatorBridge.
     * @param nativeOfflineContentAggregatorBridge A C++ pointer to the
     * OfflineContentAggregatorBridge.
     */
    private OfflineContentAggregatorBridge(long nativeOfflineContentAggregatorBridge) {
        mNativeOfflineContentAggregatorBridge = nativeOfflineContentAggregatorBridge;
        mObservers = new ObserverList<OfflineContentProvider.Observer>();
    }

    // OfflineContentProvider implementation.
    @Override
    public void openItem(OpenParams openParams, ContentId id) {
        if (mNativeOfflineContentAggregatorBridge == 0) return;
        OfflineContentAggregatorBridgeJni.get().openItem(mNativeOfflineContentAggregatorBridge,
                OfflineContentAggregatorBridge.this, openParams.location,
                openParams.openInIncognito, id.namespace, id.id);
    }

    @Override
    public void removeItem(ContentId id) {
        if (mNativeOfflineContentAggregatorBridge == 0) return;
        OfflineContentAggregatorBridgeJni.get().removeItem(mNativeOfflineContentAggregatorBridge,
                OfflineContentAggregatorBridge.this, id.namespace, id.id);
    }

    @Override
    public void cancelDownload(ContentId id) {
        if (mNativeOfflineContentAggregatorBridge == 0) return;
        OfflineContentAggregatorBridgeJni.get().cancelDownload(
                mNativeOfflineContentAggregatorBridge, OfflineContentAggregatorBridge.this,
                id.namespace, id.id);
    }

    @Override
    public void pauseDownload(ContentId id) {
        if (mNativeOfflineContentAggregatorBridge == 0) return;
        OfflineContentAggregatorBridgeJni.get().pauseDownload(mNativeOfflineContentAggregatorBridge,
                OfflineContentAggregatorBridge.this, id.namespace, id.id);
    }

    @Override
    public void resumeDownload(ContentId id, boolean hasUserGesture) {
        if (mNativeOfflineContentAggregatorBridge == 0) return;
        OfflineContentAggregatorBridgeJni.get().resumeDownload(
                mNativeOfflineContentAggregatorBridge, OfflineContentAggregatorBridge.this,
                id.namespace, id.id, hasUserGesture);
    }

    @Override
    public void getItemById(ContentId id, Callback<OfflineItem> callback) {
        if (mNativeOfflineContentAggregatorBridge == 0) return;
        OfflineContentAggregatorBridgeJni.get().getItemById(mNativeOfflineContentAggregatorBridge,
                OfflineContentAggregatorBridge.this, id.namespace, id.id, callback);
    }

    @Override
    public void getAllItems(Callback<ArrayList<OfflineItem>> callback) {
        if (mNativeOfflineContentAggregatorBridge == 0) return;
        OfflineContentAggregatorBridgeJni.get().getAllItems(mNativeOfflineContentAggregatorBridge,
                OfflineContentAggregatorBridge.this, callback);
    }

    @Override
    public void getVisualsForItem(ContentId id, VisualsCallback callback) {
        OfflineContentAggregatorBridgeJni.get().getVisualsForItem(
                mNativeOfflineContentAggregatorBridge, OfflineContentAggregatorBridge.this,
                id.namespace, id.id, callback);
    }

    @Override
    public void getShareInfoForItem(ContentId id, ShareCallback callback) {
        OfflineContentAggregatorBridgeJni.get().getShareInfoForItem(
                mNativeOfflineContentAggregatorBridge, OfflineContentAggregatorBridge.this,
                id.namespace, id.id, callback);
    }

    @Override
    public void renameItem(ContentId id, String name, Callback</*RenameResult*/ Integer> callback) {
        OfflineContentAggregatorBridgeJni.get().renameItem(mNativeOfflineContentAggregatorBridge,
                OfflineContentAggregatorBridge.this, id.namespace, id.id, name, callback);
    }

    @Override
    public void addObserver(final OfflineContentProvider.Observer observer) {
        mObservers.addObserver(observer);
    }

    @Override
    public void removeObserver(OfflineContentProvider.Observer observer) {
        mObservers.removeObserver(observer);
    }

    // Methods called from C++ via JNI.
    @CalledByNative
    private void onItemsAdded(ArrayList<OfflineItem> items) {
        if (items.size() == 0) return;

        for (Observer observer : mObservers) {
            observer.onItemsAdded(items);
        }
    }

    @CalledByNative
    private void onItemRemoved(String nameSpace, String id) {
        ContentId contentId = new ContentId(nameSpace, id);

        for (Observer observer : mObservers) {
            observer.onItemRemoved(contentId);
        }
    }

    @CalledByNative
    private void onItemUpdated(OfflineItem item, UpdateDelta updateDelta) {
        for (Observer observer : mObservers) {
            observer.onItemUpdated(item, updateDelta);
        }
    }

    @CalledByNative
    private static void onVisualsAvailable(
            VisualsCallback callback, String nameSpace, String id, OfflineItemVisuals visuals) {
        callback.onVisualsAvailable(new ContentId(nameSpace, id), visuals);
    }

    @CalledByNative
    private static void onShareInfoAvailable(
            ShareCallback callback, String nameSpace, String id, OfflineItemShareInfo shareInfo) {
        callback.onShareInfoAvailable(new ContentId(nameSpace, id), shareInfo);
    }

    /**
     * Called when the C++ OfflineContentAggregatorBridge is destroyed.  This tears down the Java
     * component of the JNI bridge so that this class, which may live due to other references, no
     * longer attempts to access the C++ side of the bridge.
     */
    @CalledByNative
    private void onNativeDestroyed() {
        mNativeOfflineContentAggregatorBridge = 0;
    }

    /**
     * A private static factory method meant to be called by the C++ OfflineContentAggregatorBridge.
     * @param nativeOfflineContentAggregatorBridge A C++ pointer to the
     * OfflineContentAggregatorBridge.
     * @return A new instance of this OfflineContentAggregatorBridge.
     */
    @CalledByNative
    private static OfflineContentAggregatorBridge create(
            long nativeOfflineContentAggregatorBridge) {
        return new OfflineContentAggregatorBridge(nativeOfflineContentAggregatorBridge);
    }

    @NativeMethods
    interface Natives {
        void openItem(long nativeOfflineContentAggregatorBridge,
                OfflineContentAggregatorBridge caller, @LaunchLocation int location,
                boolean openInIncognito, String nameSpace, String id);

        void removeItem(long nativeOfflineContentAggregatorBridge,
                OfflineContentAggregatorBridge caller, String nameSpace, String id);
        void cancelDownload(long nativeOfflineContentAggregatorBridge,
                OfflineContentAggregatorBridge caller, String nameSpace, String id);
        void pauseDownload(long nativeOfflineContentAggregatorBridge,
                OfflineContentAggregatorBridge caller, String nameSpace, String id);
        void resumeDownload(long nativeOfflineContentAggregatorBridge,
                OfflineContentAggregatorBridge caller, String nameSpace, String id,
                boolean hasUserGesture);
        void getItemById(long nativeOfflineContentAggregatorBridge,
                OfflineContentAggregatorBridge caller, String nameSpace, String id,
                Callback<OfflineItem> callback);
        void getAllItems(long nativeOfflineContentAggregatorBridge,
                OfflineContentAggregatorBridge caller, Callback<ArrayList<OfflineItem>> callback);
        void getVisualsForItem(long nativeOfflineContentAggregatorBridge,
                OfflineContentAggregatorBridge caller, String nameSpace, String id,
                VisualsCallback callback);
        void getShareInfoForItem(long nativeOfflineContentAggregatorBridge,
                OfflineContentAggregatorBridge caller, String nameSpace, String id,
                ShareCallback callback);
        void renameItem(long nativeOfflineContentAggregatorBridge,
                OfflineContentAggregatorBridge caller, String nameSpace, String id, String name,
                Callback</*RenameResult*/ Integer> callback);
    }
}
