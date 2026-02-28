// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.fusebox;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.text.TextUtils;
import android.util.ArraySet;

import androidx.annotation.VisibleForTesting;

import com.google.errorprone.annotations.MustBeClosed;

import org.chromium.base.ObserverList;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.omnibox.fusebox.ComposeboxQueryControllerBridge.FileUploadObserver;
import org.chromium.chrome.browser.omnibox.fusebox.FuseboxAttachmentRecyclerViewAdapter.FuseboxAttachmentType;
import org.chromium.chrome.browser.ui.theme.BrandedColorScheme;
import org.chromium.components.contextual_search.ContextUploadStatus;
import org.chromium.components.omnibox.OmniboxFeatures;
import org.chromium.ui.modelutil.ListObservable.ListObserver;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.SimpleRecyclerViewAdapter;

import java.util.ArrayList;
import java.util.Iterator;
import java.util.List;
import java.util.Set;
import java.util.function.Predicate;

/**
 * A specialized container for Fusebox attachments that maintains tight coupling with {@link
 * ComposeboxQueryControllerBridge}, coordinating all operations with the backend to ensure
 * consistent state.
 */
@NullMarked
public class FuseboxAttachmentModelList implements FileUploadObserver, Iterable<FuseboxAttachment> {
    private final ModelList mModelList = new ModelList();
    private final SimpleRecyclerViewAdapter mAdapter =
            new FuseboxAttachmentRecyclerViewAdapter(mModelList);

    public static int getMaxAttachments() {
        return OmniboxFeatures.sMultiattachmentFusebox.getValue() ? 10 : 1;
    }

    private final Set<Integer> mAttachedTabIds = new ArraySet<>();
    private final ObserverList<FuseboxAttachmentChangeListener> mAttachmentChangeListeners =
            new ObserverList<>();
    private @Nullable ComposeboxQueryControllerBridge mComposeboxQueryControllerBridge;
    private @BrandedColorScheme int mBrandedColorScheme;
    private @Nullable Runnable mAttachmentUploadFailedListener;
    private int mBatchEditDepth;
    private boolean mListModified;

    /**
     * Listener invoked whenever attachments list is modified. Any changes to attachments result in
     * notification being issued, including cases where attachments are reselected.
     */
    public interface FuseboxAttachmentChangeListener {
        /** Invoked whenever attachments list is changed. */
        default void onAttachmentListChanged() {}

        /** Invoked whenever attachments upload status is changed. */
        default void onAttachmentUploadStatusChanged() {}
    }

    /**
     * Batch edit guard for try-with-resources. Ensures that when batch edit completes appropriate
     * events are emitted.
     */
    /* package */ final class BatchEditToken implements AutoCloseable {
        private BatchEditToken() {
            mBatchEditDepth++;
        }

        @Override
        public void close() {
            mBatchEditDepth--;
            if (mBatchEditDepth > 0) return;
            maybeEmitListChangedEvent(/* asResultOfChange= */ false);
        }
    }

    /** Creates a new adapter for the attachments in this list. */
    public SimpleRecyclerViewAdapter getAdapter() {
        return mAdapter;
    }

    @Override
    public Iterator<FuseboxAttachment> iterator() {
        return new Iterator<>() {
            private int mIndex;

            @Override
            public boolean hasNext() {
                return mIndex < mModelList.size();
            }

            @Override
            public FuseboxAttachment next() {
                return (FuseboxAttachment) mModelList.get(mIndex++);
            }
        };
    }

    /** Returns the size of the model list. */
    public int size() {
        return mModelList.size();
    }

    /** Returns whether the model list is empty. */
    public boolean isEmpty() {
        return mModelList.isEmpty();
    }

    /** Adds a list observer. */
    public void addObserver(ListObserver<Void> observer) {
        mModelList.addObserver(observer);
    }

    /** Removes a list observer. */
    public void removeObserver(ListObserver<Void> observer) {
        mModelList.removeObserver(observer);
    }

    /** Returns the index of the FuseboxAttachment in the list. */
    @VisibleForTesting
    /* package */ int indexOf(FuseboxAttachment attachment) {
        return mModelList.indexOf(attachment);
    }

    /**
     * @param composeboxQueryControllerBridge The bridge to use for backend operations
     */
    public void setComposeboxQueryControllerBridge(
            @Nullable ComposeboxQueryControllerBridge composeboxQueryControllerBridge) {
        if (mComposeboxQueryControllerBridge == composeboxQueryControllerBridge) return;
        clear();
        if (mComposeboxQueryControllerBridge != null) {
            mComposeboxQueryControllerBridge.setFileUploadObserver(null);
        }
        mComposeboxQueryControllerBridge = composeboxQueryControllerBridge;
        if (mComposeboxQueryControllerBridge != null) {
            mComposeboxQueryControllerBridge.setFileUploadObserver(this);
        }
    }

    /**
     * Create a new batch edit token.
     *
     * <p>This method is intended to be used in a try-with-resources block to group multiple list
     * modifications. On exiting the block the token will be closed, and if any modifications were
     * made, a single change event will be emitted.
     *
     * @return A new batch edit token.
     */
    @MustBeClosed
    public BatchEditToken beginBatchEdit() {
        return new BatchEditToken();
    }

    /**
     * (Schedule a) broadcast notification informing listeners that the attachments list was
     * changed.
     *
     * <p>This method should be called from any mutator. In the event a broader set of changes are
     * expected (e.g. multiple attachments are added/removed) the change should be wrapped in a
     * try-with-resources block - see beginBatchEdit().
     *
     * @param asResultOfChange Whether the call is made because the list was changed (true), or as
     *     result of a batch edit finalization (false).
     */
    private void maybeEmitListChangedEvent(boolean asResultOfChange) {
        mListModified |= asResultOfChange;

        if (mBatchEditDepth > 0) {
            return;
        }

        if (mListModified) {
            for (var listener : mAttachmentChangeListeners) {
                listener.onAttachmentListChanged();
            }
        }

        mListModified = false;
    }

    public void setAttachmentUploadFailedListener(
            @Nullable Runnable attachmentUploadFailedListener) {
        mAttachmentUploadFailedListener = attachmentUploadFailedListener;
    }

    /** Release all resources and mark this instance ready for recycling. */
    public void destroy() {
        setComposeboxQueryControllerBridge(null);
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
        if (mComposeboxQueryControllerBridge == null || getRemainingAttachments() == 0) {
            return false;
        }

        // Start session if this is the first attachment
        if (isEmpty()) mComposeboxQueryControllerBridge.notifySessionStarted();

        // Upload the attachment if it doesn't have a token
        if (!attachment.uploadToBackend(
                mComposeboxQueryControllerBridge, /* bypassTabCacheThisTime= */ false)) {
            // Upload failed, abandon session if we just started it
            if (isEmpty()) mComposeboxQueryControllerBridge.notifySessionAbandoned();
            return false;
        }

        if (attachment.type == FuseboxAttachmentType.ATTACHMENT_TAB) {
            mAttachedTabIds.add(attachment.tabId);
        }

        attachment.model.set(FuseboxAttachmentProperties.COLOR_SCHEME, mBrandedColorScheme);
        attachment.setOnRemoveCallback(() -> remove(attachment, /* isFailure= */ false));
        mModelList.add(attachment);

        maybeEmitListChangedEvent(/* asResultOfChange= */ true);
        return true;
    }

    /**
     * Removes a FuseboxAttachment from the model list and backend. This is the preferred method for
     * removing attachments.
     *
     * @param attachment The attachment to remove
     * @param isFailure Whether the removal is from a failure or a decision by the user.
     */
    public void remove(FuseboxAttachment attachment, boolean isFailure) {
        mModelList.remove(attachment);

        if (attachment.type == FuseboxAttachmentType.ATTACHMENT_TAB) {
            mAttachedTabIds.remove(attachment.tabId);
        }

        if (isFailure) {
            FuseboxMetrics.notifyAttachmentFailed(attachment.startTime, attachment.buttonType);
        } else if (!attachment.isUploadComplete()) {
            FuseboxMetrics.notifyAttachmentAbandoned(attachment.startTime, attachment.buttonType);
        }

        // Always try to remove from backend using the model list's bridge
        // We have previously added attachments, so the controller must be set.
        attachment.removeFromBackend(assumeNonNull(mComposeboxQueryControllerBridge));
        if (isEmpty()) {
            assumeNonNull(mComposeboxQueryControllerBridge).notifySessionAbandoned();
        }
        maybeEmitListChangedEvent(/* asResultOfChange= */ true);
    }

    /**
     * Removes all tab attachments that are not explicitly identified.
     *
     * @param tabIdsToKeep A set of tab ids corresponding to tabs that are to be kept.
     */
    public void removeTabsNotInSet(Set<Integer> tabIdsToKeep) {
        removeIf(
                (ListItem item) -> {
                    if (item.type != FuseboxAttachmentType.ATTACHMENT_TAB) return false;
                    FuseboxAttachment attachment =
                            item.model.get(FuseboxAttachmentProperties.ATTACHMENT);
                    Integer tabId = assumeNonNull(attachment).tabId;
                    return !tabIdsToKeep.contains(tabId);
                });
    }

    /**
     * Removes FuseboxAttachments from the model list and backend that satisfy the given filter
     * predicate.
     *
     * @param filter The predicate to test each {@link ListItem} against for removal.
     */
    private void removeIf(Predicate<ListItem> filter) {
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
            return;
        }

        // Execute removal using the existing single-item method.
        for (FuseboxAttachment attachment : attachmentsToRemove) {
            remove(attachment, /* isFailure= */ false);
        }
    }

    /**
     * Removes all attachments from the model list and backend. This is the preferred method for
     * clearing all attachments.
     */
    public void clear() {
        if (isEmpty()) return;

        // We have previously added attachments, so the controller must be set.
        for (int index = 0; index < size(); index++) {
            var attachment = get(index);
            attachment.removeFromBackend(assumeNonNull(mComposeboxQueryControllerBridge));
            if (!attachment.isUploadComplete()) {
                FuseboxMetrics.notifyAttachmentAbandoned(
                        attachment.startTime, attachment.buttonType);
            }
        }

        mAttachedTabIds.clear();

        assumeNonNull(mComposeboxQueryControllerBridge).notifySessionAbandoned();
        mModelList.clear();
        maybeEmitListChangedEvent(/* asResultOfChange= */ true);
    }

    /**
     * Retrieves the FuseboxAttachment at the specified position in this list.
     *
     * @param index index of the element to return.
     * @return the FuseboxAttachment at the specified position.
     */
    public FuseboxAttachment get(int index) {
        return (FuseboxAttachment) mModelList.get(index);
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
    public void onFileUploadStatusChanged(String token, @ContextUploadStatus int status) {
        if (TextUtils.isEmpty(token)) return;
        FuseboxAttachment pendingAttachment = findAttachmentWithToken(token);
        if (pendingAttachment == null) return;

        switch (status) {
            case ContextUploadStatus.VALIDATION_FAILED:
            case ContextUploadStatus.UPLOAD_FAILED:
            case ContextUploadStatus.UPLOAD_EXPIRED:
                if (pendingAttachment.retryUpload(
                        assumeNonNull(mComposeboxQueryControllerBridge))) {
                    break;
                }
                notifyAttachmentUploadFailed();
                pendingAttachment.setUploadIsComplete();
                remove(pendingAttachment, /* isFailure= */ true);
                break;
            case ContextUploadStatus.UPLOAD_SUCCESSFUL:
                pendingAttachment.setUploadIsComplete();
                int index = indexOf(pendingAttachment);
                mModelList.update(index, pendingAttachment);
                FuseboxMetrics.notifyAttachmentSucceeded(
                        pendingAttachment.startTime, pendingAttachment.buttonType);
                break;
        }

        // Emit upload status changed when all attachments have been resolved.
        for (int i = 0; i < size(); i++) {
            if (!get(i).isUploadComplete()) return;
        }

        for (var listener : mAttachmentChangeListeners) {
            listener.onAttachmentUploadStatusChanged();
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
        return getMaxAttachments() - size();
    }

    private void notifyAttachmentUploadFailed() {
        if (mAttachmentUploadFailedListener != null) {
            mAttachmentUploadFailedListener.run();
        }
    }

    /** Registers the listener notified whenever attachments list is changed. */
    public void addAttachmentChangeListener(FuseboxAttachmentChangeListener listener) {
        mAttachmentChangeListeners.addObserver(listener);
    }

    /** Unregisters the listener from being notified that attachments list has been changed. */
    public void removeAttachmentChangeListener(FuseboxAttachmentChangeListener listener) {
        mAttachmentChangeListeners.removeObserver(listener);
    }

    public ModelList getModelListForTesting() {
        return mModelList;
    }
}
