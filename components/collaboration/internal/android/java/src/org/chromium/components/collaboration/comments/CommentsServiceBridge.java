// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.collaboration.comments;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.base.Callback;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.url.GURL;

import java.util.UUID;

/** Implementation of {@link CommentsService} that connects to the native counterpart. */
@JNINamespace("collaboration::comments::android")
@NullMarked
/*package*/ class CommentsServiceBridge implements CommentsService {

    private long mNativeCommentsServiceBridge;

    private CommentsServiceBridge(long nativeCommentsServiceBridge) {
        mNativeCommentsServiceBridge = nativeCommentsServiceBridge;
    }

    @CalledByNative
    private static CommentsServiceBridge create(long nativeCommentsServiceBridge) {
        return new CommentsServiceBridge(nativeCommentsServiceBridge);
    }

    @CalledByNative
    private void onNativeDestroyed() {
        mNativeCommentsServiceBridge = 0;
    }

    @Override
    public boolean isInitialized() {
        return CommentsServiceBridgeJni.get().isInitialized(mNativeCommentsServiceBridge, this);
    }

    @Override
    public boolean isEmptyService() {
        return CommentsServiceBridgeJni.get().isEmptyService(mNativeCommentsServiceBridge, this);
    }

    @Override
    public UUID addComment(
            String collaborationId,
            GURL url,
            String content,
            @Nullable UUID parentCommentId,
            Callback<Boolean> successCallback) {
        String uuid =
                CommentsServiceBridgeJni.get()
                        .addComment(
                                mNativeCommentsServiceBridge,
                                this,
                                collaborationId,
                                url,
                                content,
                                parentCommentId == null ? "" : parentCommentId.toString(),
                                successCallback);
        return UUID.fromString(uuid);
    }

    @Override
    public void editComment(UUID commentId, String newContent, Callback<Boolean> successCallback) {
        CommentsServiceBridgeJni.get()
                .editComment(
                        mNativeCommentsServiceBridge,
                        this,
                        commentId.toString(),
                        newContent,
                        successCallback);
    }

    @Override
    public void deleteComment(UUID commentId, Callback<Boolean> successCallback) {
        CommentsServiceBridgeJni.get()
                .deleteComment(
                        mNativeCommentsServiceBridge, this, commentId.toString(), successCallback);
    }

    @Override
    public void queryComments(
            CommentsService.FilterCriteria filterCriteria,
            CommentsService.PaginationCriteria paginationCriteria,
            Callback<CommentsService.QueryResult> callback) {
        CommentsServiceBridgeJni.get()
                .queryComments(
                        mNativeCommentsServiceBridge,
                        this,
                        filterCriteria,
                        paginationCriteria,
                        callback);
    }

    @Override
    public void addObserver(
            CommentsService.CommentsObserver observer,
            CommentsService.FilterCriteria filterCriteria) {
        CommentsServiceBridgeJni.get()
                .addObserver(mNativeCommentsServiceBridge, this, observer, filterCriteria);
    }

    @Override
    public void removeObserver(CommentsService.CommentsObserver observer) {
        CommentsServiceBridgeJni.get().removeObserver(mNativeCommentsServiceBridge, this, observer);
    }

    @NativeMethods
    interface Natives {
        boolean isInitialized(long nativeCommentsServiceBridge, CommentsServiceBridge caller);

        boolean isEmptyService(long nativeCommentsServiceBridge, CommentsServiceBridge caller);

        String addComment(
                long nativeCommentsServiceBridge,
                CommentsServiceBridge caller,
                String collaborationId,
                GURL url,
                String content,
                String parentCommentId,
                Callback<Boolean> successCallback);

        void editComment(
                long nativeCommentsServiceBridge,
                CommentsServiceBridge caller,
                String commentId,
                String newContent,
                Callback<Boolean> successCallback);

        void deleteComment(
                long nativeCommentsServiceBridge,
                CommentsServiceBridge caller,
                String commentId,
                Callback<Boolean> successCallback);

        void queryComments(
                long nativeCommentsServiceBridge,
                CommentsServiceBridge caller,
                CommentsService.FilterCriteria filterCriteria,
                CommentsService.PaginationCriteria paginationCriteria,
                Callback<CommentsService.QueryResult> callback);

        void addObserver(
                long nativeCommentsServiceBridge,
                CommentsServiceBridge caller,
                CommentsService.CommentsObserver observer,
                CommentsService.FilterCriteria filterCriteria);

        void removeObserver(
                long nativeCommentsServiceBridge,
                CommentsServiceBridge caller,
                CommentsService.CommentsObserver observer);
    }
}
