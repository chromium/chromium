// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.fusebox;

import org.jni_zero.CalledByNative;
import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.base.Callback;
import org.chromium.base.ResettersForTesting;
import org.chromium.base.supplier.MonotonicObservableSupplier;
import org.chromium.base.supplier.NonNullObservableSupplier;
import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.supplier.SettableMonotonicObservableSupplier;
import org.chromium.base.supplier.SettableNonNullObservableSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.components.contextual_search.ContextUploadStatus;
import org.chromium.components.contextual_search.InputState;
import org.chromium.components.omnibox.OmniboxFeatures;
import org.chromium.content_public.browser.WebContents;
import org.chromium.url.GURL;

import java.nio.ByteBuffer;
import java.util.Arrays;
import java.util.List;
import java.util.Optional;

/**
 * Bridge for native Composebox query controller functionality, allowing for management of a
 * Composebox session
 */
@SuppressWarnings("unused")
@NullMarked
public class ComposeboxQueryControllerBridge {
    /** Instance to be used for testing - null value permitted to signify no controller. */
    @SuppressWarnings("NullableOptional")
    private static @Nullable Optional<ComposeboxQueryControllerBridge> sInstanceForTesting;

    /** Observer for context upload status changes. */
    interface ContextUploadObserver {
        /**
         * @param token Unique string identifier for the context.
         * @param status The status of the context's upload.
         */
        void onContextUploadStatusChanged(String token, @ContextUploadStatus int status);
    }

    private long mNativeInstance;
    private @Nullable ContextUploadObserver mContextUploadObserver;
    private final SettableMonotonicObservableSupplier<InputState> mInputStateSupplier =
            ObservableSuppliers.createMonotonic();
    private final SettableNonNullObservableSupplier<List<SuggestedTabInfo>> mSuggestedTabsSupplier =
            ObservableSuppliers.createNonNull(List.of());

    private ComposeboxQueryControllerBridge() {}

    /**
     * Create a new ComposeboxQueryControllerBridge using the given profile and WebUI WebContents.
     *
     * @param profile The profile for the session.
     * @param webContents The WebContents hosting the WebUI that needs to be communicated with.
     * @param isTaskScoped Whether the session is scoped to a specific AI task.
     */
    public static @Nullable ComposeboxQueryControllerBridge create(
            Profile profile, @Nullable WebContents webContents, boolean isTaskScoped) {
        if (sInstanceForTesting != null) return sInstanceForTesting.orElse(null);

        ComposeboxQueryControllerBridge javaInstance = new ComposeboxQueryControllerBridge();
        long nativeInstance =
                ComposeboxQueryControllerBridgeJni.get()
                        .init(javaInstance, profile, webContents, isTaskScoped);
        if (nativeInstance == 0L) return null;
        javaInstance.mNativeInstance = nativeInstance;
        return javaInstance;
    }

    public void destroy() {
        ComposeboxQueryControllerBridgeJni.get().destroy(mNativeInstance);
        mNativeInstance = 0;
        mContextUploadObserver = null;
    }

    /** Called when the WebUI controller is destroyed. */
    public void onWebUIDestroyed() {
        if (mNativeInstance != 0) {
            ComposeboxQueryControllerBridgeJni.get().onWebUIDestroyed(mNativeInstance);
        }
    }

    public long getNativeInstance() {
        return mNativeInstance;
    }

    /**
     * Set the current context upload observer. If non-null, the observer will be notified of
     * context upload status changes for all contexts, identified by token. Note that there are
     * intermediate statuses (neither success nor failure), there is no guarantee of ordering
     * between contexts, but a context upload will either succeed/fail at most once.
     */
    void setContextUploadObserver(@Nullable ContextUploadObserver observer) {
        mContextUploadObserver = observer;
    }

    @CalledByNative
    void onContextUploadStatusChanged(String token, @ContextUploadStatus int contextUploadStatus) {
        if (mContextUploadObserver != null) {
            mContextUploadObserver.onContextUploadStatusChanged(token, contextUploadStatus);
        }
    }

    /** Start a new Composebox session. An active session is required to upload files. */
    void notifySessionStarted() {
        ComposeboxQueryControllerBridgeJni.get().notifySessionStarted(mNativeInstance);
    }

    /**
     * End the current Composebox session. This will drop all the files associated with the session.
     */
    void notifySessionAbandoned() {
        ComposeboxQueryControllerBridgeJni.get().notifySessionAbandoned(mNativeInstance);
    }

    /**
     * Add the given file to the current session.
     *
     * @return unique token representig the file, used to manipulate added files.
     */
    @Nullable String addFile(String fileName, String fileType, byte[] fileData) {
        ByteBuffer byteBuffer = ByteBuffer.allocateDirect(fileData.length);
        byteBuffer.put(fileData);
        return ComposeboxQueryControllerBridgeJni.get()
                .addFile(mNativeInstance, fileName, fileType, byteBuffer);
    }

    /**
     * Uploads the given tab, adding it to the current session. If the upload can't be performed,
     * null is returned.
     */
    @Nullable String addTabContext(Tab tab, boolean isSuggestedTab) {
        if (tab.getWebContents() == null) return null;
        return ComposeboxQueryControllerBridgeJni.get()
                .addTabContext(mNativeInstance, tab.getWebContents(), isSuggestedTab);
    }

    /**
     * Uploads the given tab, adding it to the current session. If the upload can't be performed,
     * null is returned.
     */
    @Nullable String addTabContextFromCache(long tabId, boolean isSuggestedTab) {
        return ComposeboxQueryControllerBridgeJni.get()
                .addTabContextFromCache(mNativeInstance, tabId, isSuggestedTab);
    }

    public void getAimUrl(GURL url, String queryText, Callback<GURL> callback) {
        ComposeboxQueryControllerBridgeJni.get()
                .getAimUrl(mNativeInstance, url, queryText, callback);
    }

    public void getImageGenerationUrl(GURL url, String queryText, Callback<GURL> callback) {
        ComposeboxQueryControllerBridgeJni.get()
                .getImageGenerationUrl(mNativeInstance, url, queryText, callback);
    }

    public void getAimUrlFromInputState(GURL url, String queryText, Callback<GURL> callback) {
        assert OmniboxFeatures.sShowModelPicker.getValue();
        ComposeboxQueryControllerBridgeJni.get()
                .getAimUrlFromInputState(mNativeInstance, url, queryText, callback);
    }

    /** Remove the given file from the current session. */
    void removeAttachment(String token) {
        ComposeboxQueryControllerBridgeJni.get().removeAttachment(mNativeInstance, token);
    }

    /** Returns whether client is Fusebox eligible. */
    boolean isFuseboxEligible() {
        return ComposeboxQueryControllerBridgeJni.get().isFuseboxEligible(mNativeInstance);
    }

    /** Returns whether the user is eligible for PDF uploads. */
    boolean isPdfUploadEligible() {
        return ComposeboxQueryControllerBridgeJni.get().isPdfUploadEligible(mNativeInstance);
    }

    /** Returns whether the user is eligible for creating images. */
    boolean isCreateImagesEligible() {
        return ComposeboxQueryControllerBridgeJni.get().isCreateImagesEligible(mNativeInstance);
    }

    /**
     * @param toolMode The active tool to set.
     */
    public void setActiveTool(int toolMode) {
        ComposeboxQueryControllerBridgeJni.get().setActiveTool(mNativeInstance, toolMode);
    }

    /**
     * @param modelMode The active model to set.
     */
    public void setActiveModel(int modelMode) {
        ComposeboxQueryControllerBridgeJni.get().setActiveModel(mNativeInstance, modelMode);
    }

    /**
     * Submits a query to the AI backend via postmessage to the AI page.
     *
     * @param query The query text to submit.
     */
    public void submitQueryToAimPage(String query) {
        if (mNativeInstance != 0) {
            ComposeboxQueryControllerBridgeJni.get().submitQueryToAimPage(mNativeInstance, query);
        }
    }

    /**
     * Returns an observable supplier for the current input state. This object contains the allowed
     * and disabled tools, models, and inputs. Updates are tied to the underlying C++
     * ContextualSearchSessionHandle, and may not be during other types of sessions. Callers should
     * be careful that updates may occur outside of when they expect.
     */
    public MonotonicObservableSupplier<InputState> getInputStateSupplier() {
        return mInputStateSupplier;
    }

    /** Returns an observable supplier for the suggested tab info from the backend. */
    public NonNullObservableSupplier<List<SuggestedTabInfo>> getSuggestedTabsSupplier() {
        return mSuggestedTabsSupplier;
    }

    public static void setInstanceForTesting(@Nullable ComposeboxQueryControllerBridge instance) {
        sInstanceForTesting = Optional.ofNullable(instance);
        ResettersForTesting.register(ComposeboxQueryControllerBridge::resetInstanceForTesting);
    }

    public static void resetInstanceForTesting() {
        sInstanceForTesting = null;
    }


    @CalledByNative
    private void onInputStateChanged(InputState inputState) {
        mInputStateSupplier.set(inputState);
    }

    @CalledByNative
    private void onSuggestedTabsUpdated(SuggestedTabInfo[] suggestedTabs) {
        mSuggestedTabsSupplier.set(Arrays.asList(suggestedTabs));
    }

    @NativeMethods
    public interface Natives {
        long init(
                ComposeboxQueryControllerBridge javaInstance,
                @JniType("Profile*") Profile profile,
                @JniType("content::WebContents*") @Nullable WebContents webContents,
                boolean isTaskScoped);

        void destroy(long nativeComposeboxQueryControllerBridge);

        void onWebUIDestroyed(long nativeComposeboxQueryControllerBridge);

        void notifySessionStarted(long nativeComposeboxQueryControllerBridge);

        void notifySessionAbandoned(long nativeComposeboxQueryControllerBridge);

        @Nullable String addFile(
                long nativeComposeboxQueryControllerBridge,
                @JniType("std::string") String fileName,
                @JniType("std::string") String fileType,
                ByteBuffer fileData);

        @Nullable String addTabContext(
                long nativeComposeboxQueryControllerBridge,
                @JniType("content::WebContents*") WebContents webContents,
                boolean isSuggestedTab);

        @Nullable String addTabContextFromCache(
                long nativeComposeboxQueryControllerBridge, long tabId, boolean isSuggestedTab);

        void getAimUrl(
                long nativeComposeboxQueryControllerBridge,
                @JniType("GURL") GURL url,
                @JniType("std::string") String queryText,
                Callback<GURL> callback);

        void getImageGenerationUrl(
                long nativeComposeboxQueryControllerBridge,
                @JniType("GURL") GURL url,
                @JniType("std::string") String queryText,
                Callback<GURL> callback);

        void getAimUrlFromInputState(
                long nativeComposeboxQueryControllerBridge,
                @JniType("GURL") GURL url,
                @JniType("std::string") String queryText,
                Callback<GURL> callback);

        void removeAttachment(
                long nativeComposeboxQueryControllerBridge, @JniType("std::string") String token);

        boolean isFuseboxEligible(long nativeComposeboxQueryControllerBridge);

        boolean isPdfUploadEligible(long nativeComposeboxQueryControllerBridge);

        boolean isCreateImagesEligible(long nativeComposeboxQueryControllerBridge);

        void setActiveTool(
                long nativeComposeboxQueryControllerBridge,
                @JniType("omnibox::ToolMode") int toolMode);

        void setActiveModel(
                long nativeComposeboxQueryControllerBridge,
                @JniType("omnibox::ModelMode") int modelMode);

        void submitQueryToAimPage(
                long nativeComposeboxQueryControllerBridge, @JniType("std::string") String query);
    }
}
