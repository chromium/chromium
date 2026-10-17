// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.fusebox;

import static org.chromium.build.NullUtil.assertNonNull;
import static org.chromium.build.NullUtil.assumeNonNull;

import android.content.Context;
import android.graphics.drawable.Drawable;
import android.text.TextUtils;

import androidx.appcompat.content.res.AppCompatResources;

import org.chromium.base.TimeUtils;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.omnibox.fusebox.FuseboxAttachmentRecyclerViewAdapter.FuseboxAttachmentType;
import org.chromium.chrome.browser.omnibox.fusebox.FuseboxMetrics.FuseboxAttachmentButtonType;
import org.chromium.chrome.browser.omnibox.fusebox.FuseboxMetrics.FuseboxTabAttachStartFailureReason;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.utilities.OnDemandBackgroundTabCaptureConfig;
import org.chromium.chrome.browser.tab.utilities.TabLoadingService;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.PropertyModel;

/** A complete fusebox attachment with all data and metadata. */
@NullMarked
public final class FuseboxAttachment extends ListItem {
    public final @Nullable Drawable thumbnail;
    public final String title;
    public final String mimeType;
    public final byte[] data;
    public final @Nullable Tab tab;
    public final boolean bypassTabCache;
    public final @Nullable Integer tabId;
    public final long startTime;
    public final @FuseboxAttachmentButtonType int buttonType;
    public final boolean isSuggestedTab;
    public final @Nullable String driveId;
    public final @Nullable String resourceKey;

    private boolean mIsUploadComplete;
    private boolean mIsFetchingTabDataFromCache;
    private boolean mIsPartOfMultiTabSelection;
    private @Nullable String mToken;
    private boolean mUploadSucceeded;

    private FuseboxAttachment(
            @FuseboxAttachmentType int itemType,
            @Nullable Drawable thumbnail,
            String title,
            String mimeType,
            byte[] data,
            @Nullable Tab tab,
            boolean bypassTabCache,
            @Nullable Long optionalStartTime,
            @FuseboxAttachmentButtonType int buttonType,
            boolean isSuggestedTab) {
        this(
                itemType,
                thumbnail,
                title,
                mimeType,
                data,
                tab,
                bypassTabCache,
                optionalStartTime,
                buttonType,
                isSuggestedTab,
                /* driveId= */ null,
                /* resourceKey= */ null);
    }

    private FuseboxAttachment(
            @FuseboxAttachmentType int itemType,
            @Nullable Drawable thumbnail,
            String title,
            String mimeType,
            byte[] data,
            @Nullable Tab tab,
            boolean bypassTabCache,
            @Nullable Long optionalStartTime,
            @FuseboxAttachmentButtonType int buttonType,
            boolean isSuggestedTab,
            @Nullable String driveId,
            @Nullable String resourceKey) {
        super(itemType, new PropertyModel(FuseboxAttachmentProperties.ALL_KEYS));
        this.thumbnail = thumbnail;
        this.title = title;
        this.mimeType = mimeType;
        this.data = data;
        if (tab != null && tab.getId() != Tab.INVALID_TAB_ID) {
            this.tab = tab;
            this.bypassTabCache = bypassTabCache;
            this.tabId = tab.getId();
        } else {
            this.tab = null;
            this.bypassTabCache = false;
            this.tabId = null;
        }
        mIsUploadComplete = false;
        mToken = null;
        startTime =
                optionalStartTime == null ? TimeUtils.elapsedRealtimeMillis() : optionalStartTime;
        this.buttonType = buttonType;
        this.isSuggestedTab = isSuggestedTab;
        this.driveId = driveId;
        this.resourceKey = resourceKey;

        // Set the ATTACHMENT property to this instance after construction.
        model.set(FuseboxAttachmentProperties.ATTACHMENT, this);
    }

    /** Creates a FuseboxAttachment for an image from various sources. */
    public static FuseboxAttachment forImage(
            @Nullable Drawable thumbnail,
            String title,
            String mimeType,
            byte[] data,
            long startTime,
            @FuseboxAttachmentButtonType int buttonType) {
        return new FuseboxAttachment(
                FuseboxAttachmentType.ATTACHMENT_IMAGE,
                thumbnail,
                title,
                mimeType,
                data,
                /* tab= */ null,
                /* bypassTabCache= */ false,
                startTime,
                buttonType,
                /* isSuggestedTab= */ false);
    }

    /** Creates a FuseboxAttachment for an image without a thumbnail. */
    public static FuseboxAttachment forImageNoThumbnail(
            String title,
            String mimeType,
            byte[] data,
            long startTime,
            @FuseboxAttachmentButtonType int buttonType) {
        return new FuseboxAttachment(
                FuseboxAttachmentType.ATTACHMENT_IMAGE_NO_THUMBNAIL,
                /* thumbnail= */ null,
                title,
                mimeType,
                data,
                /* tab= */ null,
                /* bypassTabCache= */ false,
                startTime,
                buttonType,
                /* isSuggestedTab= */ false);
    }

    /** Creates a FuseboxAttachment for a file. */
    public static FuseboxAttachment forFile(
            @Nullable Drawable thumbnail,
            String title,
            String mimeType,
            byte[] data,
            long startTime,
            @FuseboxAttachmentButtonType int buttonType) {
        return new FuseboxAttachment(
                FuseboxAttachmentType.ATTACHMENT_FILE,
                thumbnail,
                title,
                mimeType,
                data,
                /* tab= */ null,
                /* bypassTabCache= */ false,
                startTime,
                buttonType,
                /* isSuggestedTab= */ false);
    }

    /** Creates a FuseboxAttachment for a PDF. */
    public static FuseboxAttachment forPdf(
            @Nullable Drawable thumbnail,
            String title,
            String mimeType,
            byte[] data,
            long startTime,
            @FuseboxAttachmentButtonType int buttonType) {
        return new FuseboxAttachment(
                FuseboxAttachmentType.ATTACHMENT_PDF,
                thumbnail,
                title,
                mimeType,
                data,
                /* tab= */ null,
                /* bypassTabCache= */ false,
                startTime,
                buttonType,
                /* isSuggestedTab= */ false);
    }

    /** Creates a FuseboxAttachment for a tab. */
    public static FuseboxAttachment forTab(
            Tab tab,
            boolean bypassTabCache,
            @FuseboxAttachmentButtonType int buttonType,
            boolean isSuggestedTab) {
        // Tab thumbnails are resolved by FuseboxAttachmentViewBinder at bind time.
        return new FuseboxAttachment(
                FuseboxAttachmentType.ATTACHMENT_TAB,
                /* thumbnail= */ null,
                tab.getTitle(),
                "",
                new byte[0],
                tab,
                bypassTabCache,
                /* optionalStartTime= */ null,
                buttonType,
                isSuggestedTab);
    }

    /** Creates a FuseboxAttachment for a Drive file. */
    public static FuseboxAttachment forDrive(
            Context context,
            DriveAttachmentMetadata metadata,
            long startTime,
            @FuseboxAttachmentButtonType int buttonType) {
        int iconRes = DriveIconUtils.getIconForDriveFile(metadata.mimeType, metadata.title);
        Drawable icon = AppCompatResources.getDrawable(context, iconRes);
        return new FuseboxAttachment(
                FuseboxAttachmentType.ATTACHMENT_DRIVE,
                icon,
                metadata.title,
                metadata.mimeType,
                new byte[0],
                /* tab= */ null,
                /* bypassTabCache= */ false,
                startTime,
                buttonType,
                /* isSuggestedTab= */ false,
                metadata.driveId,
                metadata.resourceKey);
    }

    /**
     * Uploads this attachment using the provided bridge and sets its token.
     *
     * <p>A started upload, including a retry of one that already completed, leaves this attachment
     * incomplete until the backend reports a terminal status for it.
     *
     * @param bridge The bridge to use for uploading
     * @param bypassTabCacheThisTime Whether to bypass the tab cache.
     * @return Whether the upload started.
     */
    /* package */ boolean uploadToBackend(
            ComposeboxQueryControllerBridge bridge, boolean bypassTabCacheThisTime) {
        assert !hasToken() || bypassTabCacheThisTime
                : "Attachment should not have a token when uploaded except for tab data retries";

        boolean started;
        if (type == FuseboxAttachmentType.ATTACHMENT_TAB) {
            started = startTabUpload(bridge, bypassTabCacheThisTime);
        } else {
            mToken =
                    type == FuseboxAttachmentType.ATTACHMENT_DRIVE
                            ? bridge.addDriveFile(
                                    assumeNonNull(driveId), resourceKey, title, mimeType)
                            : bridge.addFile(title, mimeType, data);
            started = hasToken();
        }
        if (started) mIsUploadComplete = false;
        return started;
    }

    /**
     * Starts the upload of this tab attachment, recording why it could not start when it did not.
     *
     * @param bridge The bridge to use for uploading
     * @param bypassTabCacheThisTime Whether to bypass the tab cache.
     * @return Whether the upload started.
     */
    private boolean startTabUpload(
            ComposeboxQueryControllerBridge bridge, boolean bypassTabCacheThisTime) {
        boolean isRetry = hasToken();
        mIsFetchingTabDataFromCache = false;

        // We must fetch fresh tab content for the active tab, incognito tabs (not in cache), or
        // if it is forced.
        boolean mustFetchFreshTabContent =
                (bypassTabCache
                        || assumeNonNull(tab).isIncognitoBranded()
                        || bypassTabCacheThisTime);

        // We can fetch fresh tab content for active tabs.
        boolean canFetchFreshTabContent = FuseboxTabUtils.isTabActive(tab);

        if (mustFetchFreshTabContent && canFetchFreshTabContent) {
            mToken = bridge.addTabContext(assumeNonNull(tab), isSuggestedTab);
        } else if (mustFetchFreshTabContent && !canFetchFreshTabContent) {
            // We must do a fresh fetch and cannot rely on the cache, but it is not possible to
            // perform a fresh fetch. The upload will fail.
            recordStartFailureUnlessRetry(
                    FuseboxTabAttachStartFailureReason.FRESH_REQUIRED_TAB_NOT_LIVE, isRetry);
            return false;
        } else {
            // Try the cache first.
            mIsFetchingTabDataFromCache = true;
            mToken = bridge.addTabContextFromCache(getTabId(), isSuggestedTab);

            // If cache fetch fails, try to fetch fresh data if possible.
            if (TextUtils.isEmpty(mToken) && canFetchFreshTabContent) {
                // We are not trying to fetch from the cache anymore we are doing a fresh fetch
                // instead.
                mIsFetchingTabDataFromCache = false;

                mToken = bridge.addTabContext(assumeNonNull(tab), isSuggestedTab);
            } else if (TextUtils.isEmpty(mToken)) {
                recordStartFailureUnlessRetry(
                        FuseboxTabAttachStartFailureReason.CACHE_UNAVAILABLE_TAB_NOT_LIVE, isRetry);
                return false;
            }
        }

        if (!hasToken()) {
            recordStartFailureUnlessRetry(
                    FuseboxTabAttachStartFailureReason.UPLOAD_REJECTED, isRetry);
            return false;
        }
        return true;
    }

    /**
     * Records a tab attach start failure unless the attempt was a retry. A retry re-enters with the
     * token of its earlier upload; it is not a new attach attempt, so its rejection is not a start
     * failure.
     */
    private void recordStartFailureUnlessRetry(
            @FuseboxTabAttachStartFailureReason int reason, boolean isRetry) {
        if (isRetry) return;
        FuseboxMetrics.recordTabAttachStartFailure(reason, buttonType);
    }

    /** Returns whether the backend has ever reported a successful upload for this attachment. */
    /* package */ boolean hasUploadSucceeded() {
        return mUploadSucceeded;
    }

    /** Marks the current upload as successful and complete. */
    /* package */ void markUploadSucceeded() {
        mUploadSucceeded = true;
        mIsUploadComplete = true;
    }

    /**
     * Returns whether the user is still waiting on this attachment's upload. Suggested tabs are
     * attached automatically and upload in the background, so they are never reported as awaited.
     */
    /* package */ boolean isAwaitingUpload() {
        return !mIsUploadComplete && !isSuggestedTab;
    }

    /**
     * Returns whether removing this attachment now, other than for a failure, abandons it: the user
     * attached it and its upload never succeeded. Suggested tabs are added and swapped out
     * automatically, so discarding one is not abandonment.
     */
    /* package */ boolean isAbandonedIfRemoved() {
        return !mUploadSucceeded && !isSuggestedTab;
    }

    /**
     * Removes this attachment from the backend using the provided bridge.
     *
     * @param bridge The bridge to use for removal
     */
    public void removeFromBackend(ComposeboxQueryControllerBridge bridge) {
        if (tab != null
                && !isSuggestedTab
                && OnDemandBackgroundTabCaptureConfig.isCancelLoadOnDeselectionEnabled()) {
            TabLoadingService.getInstance().cancelLoadIfNeeded(tab);
        }
        if (hasToken()) {
            bridge.removeAttachment(getToken());
        }
    }

    /** Sets the ON_REMOVE callback for this attachment. */
    /* package */ void setOnRemoveCallback(Runnable onRemove) {
        model.set(FuseboxAttachmentProperties.ON_REMOVE, onRemove);
    }

    /** Checks if this attachment has a token. */
    public boolean hasToken() {
        return !TextUtils.isEmpty(mToken);
    }

    /** Gets the token for this attachment. */
    public String getToken() {
        return assertNonNull(mToken);
    }

    public boolean isUploadComplete() {
        return mIsUploadComplete;
    }

    public void setUploadIsComplete() {
        mIsUploadComplete = true;
    }

    /** Sets whether this tab was one of several newly attached by one tab picker selection. */
    void setIsPartOfMultiTabSelection(boolean isPartOfMultiTabSelection) {
        mIsPartOfMultiTabSelection = isPartOfMultiTabSelection;
    }

    /** Returns whether this tab was one of several newly attached by one tab picker selection. */
    boolean isPartOfMultiTabSelection() {
        return mIsPartOfMultiTabSelection;
    }

    /** Returns the ID of the associated tab, or Tab.INVALID_TAB_ID if no tab is associated. */
    public int getTabId() {
        return tabId != null ? tabId : Tab.INVALID_TAB_ID;
    }

    public boolean retryUpload(ComposeboxQueryControllerBridge composeBoxQueryControllerBridge) {
        if (type == FuseboxAttachmentType.ATTACHMENT_TAB && mIsFetchingTabDataFromCache) {
            return uploadToBackend(
                    assumeNonNull(composeBoxQueryControllerBridge),
                    /* bypassTabCacheThisTime= */ true);
        }
        return false;
    }
}
