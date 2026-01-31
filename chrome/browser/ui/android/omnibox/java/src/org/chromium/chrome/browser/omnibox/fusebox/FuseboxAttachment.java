// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.fusebox;

import static org.chromium.build.NullUtil.assertNonNull;
import static org.chromium.build.NullUtil.assumeNonNull;

import android.content.res.Resources;
import android.graphics.drawable.BitmapDrawable;
import android.graphics.drawable.Drawable;
import android.text.TextUtils;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.omnibox.fusebox.FuseboxAttachmentRecyclerViewAdapter.FuseboxAttachmentType;
import org.chromium.chrome.browser.omnibox.styles.OmniboxResourceProvider;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
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
    public final @Nullable Integer tabId;
    private boolean mIsUploadComplete;
    private boolean mIsFetchingTabDataFromCache;
    private @Nullable String mToken;

    private FuseboxAttachment(
            @FuseboxAttachmentType int itemType,
            @Nullable Drawable thumbnail,
            String title,
            String mimeType,
            byte[] data,
            @Nullable Tab tab) {
        super(itemType, new PropertyModel(FuseboxAttachmentProperties.ALL_KEYS));
        this.thumbnail = thumbnail;
        this.title = title;
        this.mimeType = mimeType;
        this.data = data;
        if (tab != null && tab.getId() != Tab.INVALID_TAB_ID) {
            this.tab = tab;
            this.tabId = tab.getId();
        } else {
            this.tab = null;
            this.tabId = null;
        }
        mIsUploadComplete = false;
        mToken = null;

        // Set the ATTACHMENT property to this instance after construction
        model.set(FuseboxAttachmentProperties.ATTACHMENT, this);
    }

    /** Creates a FuseboxAttachment for a camera image. */
    public static FuseboxAttachment forCameraImage(
            @Nullable Drawable thumbnail, String title, String mimeType, byte[] data) {
        return new FuseboxAttachment(
                FuseboxAttachmentType.ATTACHMENT_IMAGE, thumbnail, title, mimeType, data, null);
    }

    /** Creates a FuseboxAttachment for a file. */
    public static FuseboxAttachment forFile(
            @Nullable Drawable thumbnail, String title, String mimeType, byte[] data) {
        return new FuseboxAttachment(
                FuseboxAttachmentType.ATTACHMENT_FILE, thumbnail, title, mimeType, data, null);
    }

    /** Creates a FuseboxAttachment for a tab. */
    public static FuseboxAttachment forTab(Tab tab, Resources res) {
        return new FuseboxAttachment(
                FuseboxAttachmentType.ATTACHMENT_TAB,
                new BitmapDrawable(res, OmniboxResourceProvider.getFaviconBitmapForTab(tab)),
                tab.getTitle(),
                "",
                new byte[0],
                tab);
    }

    /**
     * Uploads this attachment using the provided bridge and sets its token.
     *
     * @param bridge The bridge to use for uploading
     * @param currentlySelectedTab The currently selected tab, if any.
     * @param forceFreshTabFetch Whether to to bypass the cache for a Tab attachment.
     * @return true if upload succeeded, false otherwise
     */
    /* package */ boolean uploadToBackend(
            ComposeBoxQueryControllerBridge bridge,
            @Nullable Tab currentlySelectedTab,
            boolean forceFreshTabFetch) {
        assert !hasToken() || forceFreshTabFetch
                : "Attachment should not have a token when uploaded except for tab data retries";

        if (type == FuseboxAttachmentType.ATTACHMENT_TAB) {
            mIsFetchingTabDataFromCache = false;
            if (FuseboxTabUtils.isTabActive(tab)
                    && (tab == currentlySelectedTab
                            // There is no cache for incognito tabs.
                            || assumeNonNull(tab).isIncognitoBranded()
                            || forceFreshTabFetch)) {
                mToken = bridge.addTabContext(assumeNonNull(tab));
            } else if (forceFreshTabFetch) {
                // The caller asked for a fresh fetch and we can't give them one; upload cannot
                // succeed.
                return false;
            } else {
                mIsFetchingTabDataFromCache = true;
                mToken = bridge.addTabContextFromCache(assumeNonNull(tab).getId());
                // If cache fetch fails, try to fetch fresh data.
                if (TextUtils.isEmpty(mToken) && FuseboxTabUtils.isTabActive(tab)) {
                    mIsFetchingTabDataFromCache = false;
                    mToken = bridge.addTabContext(assumeNonNull(tab));
                }
            }
        } else {
            mToken = bridge.addFile(title, mimeType, data);
        }

        return !TextUtils.isEmpty(mToken);
    }

    /**
     * Removes this attachment from the backend using the provided bridge.
     *
     * @param bridge The bridge to use for removal
     */
    public void removeFromBackend(ComposeBoxQueryControllerBridge bridge) {
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

    public boolean retryUpload(
            @Nullable TabModelSelector tabModelSelector,
            ComposeBoxQueryControllerBridge composeBoxQueryControllerBridge) {
        if (type == FuseboxAttachmentType.ATTACHMENT_TAB && mIsFetchingTabDataFromCache) {
            // Fetch from cache can fail with a delay. Try to fetch fresh data instead of
            // giving up entirely.
            @Nullable Tab currentlySelectedTab =
                    tabModelSelector != null ? tabModelSelector.getCurrentTab() : null;
            uploadToBackend(
                    assumeNonNull(composeBoxQueryControllerBridge), currentlySelectedTab, true);
            return true;
        }
        return false;
    }
}
