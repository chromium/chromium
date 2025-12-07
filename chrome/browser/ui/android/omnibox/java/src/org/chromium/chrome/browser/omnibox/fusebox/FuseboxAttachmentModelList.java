// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.fusebox;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.text.TextUtils;
import android.util.ArraySet;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.omnibox.fusebox.ComposeBoxQueryControllerBridge.FileUploadObserver;
import org.chromium.chrome.browser.omnibox.fusebox.FuseboxAttachmentRecyclerViewAdapter.FuseboxAttachmentType;
import org.chromium.chrome.browser.ui.theme.BrandedColorScheme;
import org.chromium.components.contextual_search.FileUploadStatus;
import org.chromium.components.omnibox.OmniboxFeatures;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;

import java.util.ArrayList;
import java.util.List;
import java.util.Set;
import java.util.function.Predicate;

/**
 * A specialized ModelList for Fusebox attachments that maintains tight coupling with
 * ComposeBoxQueryController, coordinating all operations with the backend to ensure consistent
 * state.
 */
@NullMarked
public class FuseboxAttachmentModelList extends ModelList implements FileUploadObserver {
    static final int MAX_ATTACHMENTS = OmniboxFeatures.sMultiattachmentFusebox.getValue() ? 10 : 1;
    private final Set<Integer> mAttachedTabIds = new ArraySet<>();
    private @Nullable ComposeBoxQueryControllerBridge mComposeBoxQueryControllerBridge;
    private @BrandedColorScheme int mBrandedColorScheme;
    private @Nullable Runnable mAttachmentUploadFailedListener;

    /**
     * @param composeBoxQueryControllerBridge The bridge to use for backend operations
     */
    public void setComposeBoxQueryControllerBridge(
            @Nullable ComposeBoxQueryControllerBridge composeBoxQueryControllerBridge) {
        if (mComposeBoxQueryControllerBridge == composeBoxQueryControllerBridge) return;
        clear();
        if (mComposeBoxQueryControllerBridge != null) {
            mComposeBoxQueryControllerBridge.setFileUploadObserver(null);
        }
        mComposeBoxQueryControllerBridge = composeBoxQueryControllerBridge;
        if (mComposeBoxQueryControllerBridge != null) {
            mComposeBoxQueryControllerBridge.setFileUploadObserver(this);
        }
    }

    public void setAttachmentUploadFailedListener(
            @Nullable Runnable attachmentUploadFailedListener) {
        mAttachmentUploadFailedListener = attachmentUploadFailedListener;
    }

    /** Release all resources and mark this instance ready for recycling. */
    void destroy() {
        setComposeBoxQueryControllerBridge(null);
        mAttachmentUploadFailedListener = null;
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
    public boolean add(FuseboxAttachment attachment) {
        if (mComposeBoxQueryControllerBridge == null || getRemainingAttachments() == 0) {
            return false;
        }

        // Start session if this is the first attachment
        if (isEmpty()) mComposeBoxQueryControllerBridge.notifySessionStarted();

        // Upload the attachment if it doesn't have a token
        if (!attachment.uploadToBackend(mComposeBoxQueryControllerBridge)) {
            // Upload failed, abandon session if we just started it
            if (isEmpty()) mComposeBoxQueryControllerBridge.notifySessionAbandoned();
            return false;
        }

        if (attachment.type == FuseboxAttachmentType.ATTACHMENT_TAB) {
            mAttachedTabIds.add(attachment.tabId);
        }

        attachment.model.set(FuseboxAttachmentProperties.COLOR_SCHEME, mBrandedColorScheme);
        attachment.setOnRemoveCallback(() -> remove(attachment));
        super.add(attachment);
        return true;
    }

    @Override
    public void add(ListItem item) {
        throw new IllegalArgumentException(
                "Use the boolean add() version to ensure capacity constraints are respected.");
    }

    /**
     * Removes a FuseboxAttachment from the model list and backend. This is the preferred method for
     * removing attachments.
     *
     * @param attachment The attachment to remove
     */
    public void remove(FuseboxAttachment attachment) {
        super.remove(attachment);

        if (attachment.type == FuseboxAttachmentType.ATTACHMENT_TAB) {
            mAttachedTabIds.remove(attachment.tabId);
        }

        // Always try to remove from backend using the model list's bridge
        // We have previously added attachments, so the controller must be set.
        attachment.removeFromBackend(assumeNonNull(mComposeBoxQueryControllerBridge));
        if (isEmpty()) {
            assumeNonNull(mComposeBoxQueryControllerBridge).notifySessionAbandoned();
        }
    }

    /**
     * Removes FuseboxAttachments from the model list and backend that satisfy the given filter
     * predicate.
     *
     * @param filter The predicate to test each {@link ListItem} against for removal.
     * @return True if one or more attachments were removed; False, otherwise.
     */
    public boolean removeIf(Predicate<ListItem> filter) {
        List<FuseboxAttachment> attachmentsToRemove = new ArrayList<>();

        // Identify attachments that satisfy the filter.
        for (int index = 0; index < size(); index++) {
            FuseboxAttachment attachment = get(index);
            if (filter.test(attachment)) {
                attachmentsToRemove.add(attachment);
            }
        }

        // If nothing was found, return false.
        if (attachmentsToRemove.isEmpty()) {
            return false;
        }

        // Execute removal using the existing single-item method.
        for (FuseboxAttachment attachment : attachmentsToRemove) {
            remove(attachment);
        }

        // Since we executed actual removal logic one or more times, return true.
        return true;
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

        mAttachedTabIds.clear();

        assumeNonNull(mComposeBoxQueryControllerBridge).notifySessionAbandoned();
        super.clear();
    }

    /**
     * Retrieves the FuseboxAttachment at the specified position in this list.
     *
     * @param index index of the element to return.
     * @return the FuseboxAttachment at the specified position.
     */
    @Override
    public FuseboxAttachment get(int index) {
        return (FuseboxAttachment) super.get(index);
    }

    /** Returns a set of currently attached Tab IDs. */
    public Set<Integer> getAttachedTabIds() {
        return mAttachedTabIds;
    }

    /** Apply a variant of the branded color scheme to Fusebox Attachment elements. */
    /*package */ void updateVisualsForState(@BrandedColorScheme int brandedColorScheme) {
        if (mBrandedColorScheme == brandedColorScheme) return;
        mBrandedColorScheme = brandedColorScheme;
        for (var item : this) {
            item.model.set(FuseboxAttachmentProperties.COLOR_SCHEME, brandedColorScheme);
        }
    }

    @Override
    public void onFileUploadStatusChanged(String token, @FileUploadStatus int status) {
        if (TextUtils.isEmpty(token)) return;
        FuseboxAttachment pendingAttachment = findAttachmentWithToken(token);
        if (pendingAttachment == null) return;

        switch (status) {
            case FileUploadStatus.VALIDATION_FAILED:
            case FileUploadStatus.UPLOAD_FAILED:
            case FileUploadStatus.UPLOAD_EXPIRED:
                notifyAttachmentUploadFailed();
                pendingAttachment.setUploadIsComplete();
                remove(pendingAttachment);
                break;
            case FileUploadStatus.UPLOAD_SUCCESSFUL:
                pendingAttachment.setUploadIsComplete();
                notifyItemChanged(indexOf(pendingAttachment));
                break;
        }
    }

    private @Nullable FuseboxAttachment findAttachmentWithToken(String token) {
        for (int i = 0; i < size(); i++) {
            FuseboxAttachment attachment = get(i);
            if (TextUtils.equals(token, attachment.getToken())) {
                return attachment;
            }
        }
        return null;
    }

    /**
     * Returns the number of attachments that generally can be added. Does not have any
     * understanding of the current request type/tool, and thus does not factor that into
     * calculations. Instead any caller will need to make appropriate decisions about this instead
     * if needed.
     */
    public int getRemainingAttachments() {
        return MAX_ATTACHMENTS - size();
    }

    private void notifyAttachmentUploadFailed() {
        if (mAttachmentUploadFailedListener != null) {
            mAttachmentUploadFailedListener.run();
        }
    }
}
