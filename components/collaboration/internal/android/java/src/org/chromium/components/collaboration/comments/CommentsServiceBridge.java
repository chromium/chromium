// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.collaboration.comments;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.JniType;
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
        return CommentsServiceBridgeJni.get().isInitialized(mNativeCommentsServiceBridge);
    }

    @Override
    public boolean isEmptyService() {
        return CommentsServiceBridgeJni.get().isEmptyService(mNativeCommentsServiceBridge);
    }

    @Override
    public UUID addComment(
            String collaborationId,
            GURL url,
            String content,
            @Nullable UUID parentCommentId,
            Callback<Boolean> successCallback) {
        return CommentsServiceBridgeJni.get()
                .addComment(
                        mNativeCommentsServiceBridge,
                        collaborationId,
                        url,
                        content,
                        parentCommentId,
                        successCallback);
    }

    @Override
    public void editComment(UUID commentId, String newContent, Callback<Boolean> successCallback) {
        CommentsServiceBridgeJni.get()
                .editComment(mNativeCommentsServiceBridge, commentId, newContent, successCallback);
    }

    @Override
    public void deleteComment(UUID commentId, Callback<Boolean> successCallback) {
        CommentsServiceBridgeJni.get()
                .deleteComment(mNativeCommentsServiceBridge, commentId, successCallback);
    }

    @Override
    public void queryComments(
            CommentsService.FilterCriteria filterCriteria,
            CommentsService.PaginationCriteria paginationCriteria,
            Callback<CommentsService.QueryResult> callback) {
        CommentsServiceBridgeJni.get()
                .queryComments(
                        mNativeCommentsServiceBridge, filterCriteria, paginationCriteria, callback);
    }

    @Override
    public void addObserver(
            CommentsService.CommentsObserver observer,
            CommentsService.FilterCriteria filterCriteria) {
        CommentsServiceBridgeJni.get()
                .addObserver(mNativeCommentsServiceBridge, observer, filterCriteria);
    }

    @Override
    public void removeObserver(CommentsService.CommentsObserver observer) {
        CommentsServiceBridgeJni.get().removeObserver(mNativeCommentsServiceBridge, observer);
    }

    @NativeMethods
    interface Natives {
        boolean isInitialized(long nativeCommentsServiceBridge);

        boolean isEmptyService(long nativeCommentsServiceBridge);

        @JniType("base::Uuid")
        UUID addComment(
                long nativeCommentsServiceBridge,
                @JniType("std::string") String collaborationId,
                @JniType("GURL") GURL url,
                @JniType("std::string") String content,
                @JniType("std::optional<base::Uuid>") @Nullable UUID parentCommentId,
                Callback<Boolean> successCallback);

        void editComment(
                long nativeCommentsServiceBridge,
                @JniType("base::Uuid") UUID commentId,
                @JniType("std::string") String newContent,
                Callback<Boolean> successCallback);

        void deleteComment(
                long nativeCommentsServiceBridge,
                @JniType("base::Uuid") UUID commentId,
                Callback<Boolean> successCallback);

        void queryComments(
                long nativeCommentsServiceBridge,
                CommentsService.FilterCriteria filterCriteria,
                CommentsService.PaginationCriteria paginationCriteria,
                Callback<CommentsService.QueryResult> callback);

        void addObserver(
                long nativeCommentsServiceBridge,
                CommentsService.CommentsObserver observer,
                CommentsService.FilterCriteria filterCriteria);

        void removeObserver(
                long nativeCommentsServiceBridge, CommentsService.CommentsObserver observer);
    }
}
