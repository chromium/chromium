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
     * @return true if upload succeeded, false otherwise
     */
    /* package */ boolean uploadToBackend(ComposeBoxQueryControllerBridge bridge) {
        assert !hasToken() : "Attachment should not have a token when uploaded";

        if (type == FuseboxAttachmentType.ATTACHMENT_TAB) {
            if (FuseboxTabUtils.isTabActive(assumeNonNull(tab))) {
                mToken = bridge.addTabContext(tab);
            } else {
                mToken = bridge.addTabContextFromCache(assumeNonNull(tab).getId());
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
}
