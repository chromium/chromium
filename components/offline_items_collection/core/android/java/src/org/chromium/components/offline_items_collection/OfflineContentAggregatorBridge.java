// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.offline_items_collection;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.base.Callback;
import org.chromium.base.ObserverList;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

import java.util.ArrayList;

/**
 * A helper class responsible for exposing the C++ OfflineContentAggregator
 * (components/offline_items_collection/core/offline_content_aggregator.h) class to Java.  This
 * class is created and owned by it's C++ counterpart OfflineContentAggregatorBridge
 * (components/offline_items_collection/core/android/offline_content_aggregator_bridge.h).
 */
@JNINamespace("offline_items_collection::android")
@NullMarked
public class OfflineContentAggregatorBridge implements OfflineContentProvider {
    private long mNativeOfflineContentAggregatorBridge;
    private final ObserverList<OfflineContentProvider.Observer> mObservers;

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
        OfflineContentAggregatorBridgeJni.get()
                .openItem(
                        mNativeOfflineContentAggregatorBridge,
                        openParams.location,
                        openParams.openInIncognito,
                        id.namespace,
                        id.id);
    }

    @Override
    public void removeItem(ContentId id) {
        if (mNativeOfflineContentAggregatorBridge == 0) return;
        OfflineContentAggregatorBridgeJni.get()
                .removeItem(mNativeOfflineContentAggregatorBridge, id.namespace, id.id);
    }

    @Override
    public void cancelDownload(ContentId id) {
        if (mNativeOfflineContentAggregatorBridge == 0) return;
        OfflineContentAggregatorBridgeJni.get()
                .cancelDownload(mNativeOfflineContentAggregatorBridge, id.namespace, id.id);
    }

    @Override
    public void pauseDownload(ContentId id) {
        if (mNativeOfflineContentAggregatorBridge == 0) return;
        OfflineContentAggregatorBridgeJni.get()
                .pauseDownload(mNativeOfflineContentAggregatorBridge, id.namespace, id.id);
    }

    @Override
    public void resumeDownload(ContentId id) {
        if (mNativeOfflineContentAggregatorBridge == 0) return;
        OfflineContentAggregatorBridgeJni.get()
                .resumeDownload(mNativeOfflineContentAggregatorBridge, id.namespace, id.id);
    }

    @Override
    public void validateDangerousDownload(ContentId id) {
        if (mNativeOfflineContentAggregatorBridge == 0) return;
        OfflineContentAggregatorBridgeJni.get()
                .validateDangerousDownload(
                        mNativeOfflineContentAggregatorBridge, id.namespace, id.id);
    }

    @Override
    public void getItemById(ContentId id, Callback<OfflineItem> callback) {
        if (mNativeOfflineContentAggregatorBridge == 0) return;
        OfflineContentAggregatorBridgeJni.get()
                .getItemById(mNativeOfflineContentAggregatorBridge, id.namespace, id.id, callback);
    }

    @Override
    public void getAllItems(Callback<ArrayList<OfflineItem>> callback) {
        if (mNativeOfflineContentAggregatorBridge == 0) return;
        OfflineContentAggregatorBridgeJni.get()
                .getAllItems(mNativeOfflineContentAggregatorBridge, callback);
    }

    @Override
    public void getVisualsForItem(ContentId id, VisualsCallback callback) {
        OfflineContentAggregatorBridgeJni.get()
                .getVisualsForItem(
                        mNativeOfflineContentAggregatorBridge, id.namespace, id.id, callback);
    }

    @Override
    public void getShareInfoForItem(ContentId id, ShareCallback callback) {
        OfflineContentAggregatorBridgeJni.get()
                .getShareInfoForItem(
                        mNativeOfflineContentAggregatorBridge, id.namespace, id.id, callback);
    }

    @Override
    public void renameItem(ContentId id, String name, Callback</*RenameResult*/ Integer> callback) {
        OfflineContentAggregatorBridgeJni.get()
                .renameItem(
                        mNativeOfflineContentAggregatorBridge, id.namespace, id.id, name, callback);
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
        void openItem(
                long nativeOfflineContentAggregatorBridge,
                @LaunchLocation int location,
                boolean openInIncognito,
                @Nullable String nameSpace,
                @Nullable String id);

        void removeItem(
                long nativeOfflineContentAggregatorBridge,
                @Nullable String nameSpace,
                @Nullable String id);

        void cancelDownload(
                long nativeOfflineContentAggregatorBridge,
                @Nullable String nameSpace,
                @Nullable String id);

        void pauseDownload(
                long nativeOfflineContentAggregatorBridge,
                @Nullable String nameSpace,
                @Nullable String id);

        void resumeDownload(
                long nativeOfflineContentAggregatorBridge,
                @Nullable String nameSpace,
                @Nullable String id);

        void validateDangerousDownload(
                long nativeOfflineContentAggregatorBridge,
                @Nullable String nameSpace,
                @Nullable String id);

        void getItemById(
                long nativeOfflineContentAggregatorBridge,
                @Nullable String nameSpace,
                @Nullable String id,
                Callback<OfflineItem> callback);

        void getAllItems(
                long nativeOfflineContentAggregatorBridge,
                Callback<ArrayList<OfflineItem>> callback);

        void getVisualsForItem(
                long nativeOfflineContentAggregatorBridge,
                @Nullable String nameSpace,
                @Nullable String id,
                VisualsCallback callback);

        void getShareInfoForItem(
                long nativeOfflineContentAggregatorBridge,
                @Nullable String nameSpace,
                @Nullable String id,
                ShareCallback callback);

        void renameItem(
                long nativeOfflineContentAggregatorBridge,
                @Nullable String nameSpace,
                @Nullable String id,
                String name,
                Callback</*RenameResult*/ Integer> callback);
    }
}
