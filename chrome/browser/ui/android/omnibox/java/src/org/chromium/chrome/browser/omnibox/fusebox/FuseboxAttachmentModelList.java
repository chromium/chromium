// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.fusebox;

import static org.chromium.build.NullUtil.assumeNonNull;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.ui.theme.BrandedColorScheme;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;

/**
 * A specialized ModelList for Fusebox attachments that maintains tight coupling with
 * ComposeBoxQueryController, coordinating all operations with the backend to ensure consistent
 * state.
 */
@NullMarked
public class FuseboxAttachmentModelList extends ModelList {
    private @Nullable ComposeBoxQueryControllerBridge mComposeBoxQueryControllerBridge;
    private @BrandedColorScheme int mBrandedColorScheme;

    /**
     * @param composeBoxQueryControllerBridge The bridge to use for backend operations
     */
    public void setComposeBoxQueryControllerBridge(
            @Nullable ComposeBoxQueryControllerBridge composeBoxQueryControllerBridge) {
        if (mComposeBoxQueryControllerBridge == composeBoxQueryControllerBridge) return;
        clear();
        mComposeBoxQueryControllerBridge = composeBoxQueryControllerBridge;
    }

    /** Release all resources and mark this instance ready for recycling. */
    void destroy() {
        setComposeBoxQueryControllerBridge(null);
    }

    /**
     * @return true if a session is currently active
     */
    public boolean isSessionStarted() {
        return !isEmpty();
    }

    /**
     * Adds a FuseboxAttachment directly, uploading it first if needed. This is the preferred method
     * for adding attachments.
     *
     * @param attachment The attachment to add
     */
    public void add(FuseboxAttachment attachment) {
        if (mComposeBoxQueryControllerBridge == null) return;

        // Start session if this is the first attachment
        if (isEmpty()) mComposeBoxQueryControllerBridge.notifySessionStarted();

        // Upload the attachment if it doesn't have a token
        if (!attachment.uploadToBackend(mComposeBoxQueryControllerBridge)) {
            // Upload failed, abandon session if we just started it
            if (isEmpty()) mComposeBoxQueryControllerBridge.notifySessionAbandoned();
            return;
        }

        attachment.model.set(FuseboxAttachmentProperties.COLOR_SCHEME, mBrandedColorScheme);
        attachment.setOnRemoveCallback(() -> remove(attachment));
        super.add(attachment);
    }

    /**
     * Removes a FuseboxAttachment from the model list and backend. This is the preferred method for
     * removing attachments.
     *
     * @param attachment The attachment to remove
     */
    public void remove(FuseboxAttachment attachment) {
        super.remove(attachment);

        // Always try to remove from backend using the model list's bridge
        // We have previously added attachments, so the controller must be set.
        attachment.removeFromBackend(assumeNonNull(mComposeBoxQueryControllerBridge));
        if (isEmpty()) {
            assumeNonNull(mComposeBoxQueryControllerBridge).notifySessionAbandoned();
        }
    }

    /**
     * Removes all attachments from the model list and backend. This is the preferred method for
     * clearing all attachments.
     */
    @Override
    public void clear() {
        if (isEmpty()) return;

        // We have previously added attachments, so the controller must be set.
        for (int index = 0; index < size(); index++) {
            var attachment = (FuseboxAttachment) get(index);
            attachment.removeFromBackend(assumeNonNull(mComposeBoxQueryControllerBridge));
        }

        assumeNonNull(mComposeBoxQueryControllerBridge).notifySessionAbandoned();
        super.clear();
    }

    /** Apply a variant of the branded color scheme to Fusebox Attachment elements. */
    /*package */ void updateVisualsForState(@BrandedColorScheme int brandedColorScheme) {
        if (mBrandedColorScheme == brandedColorScheme) return;
        mBrandedColorScheme = brandedColorScheme;
        for (var item : this) {
            item.model.set(FuseboxAttachmentProperties.COLOR_SCHEME, brandedColorScheme);
        }
    }
}
