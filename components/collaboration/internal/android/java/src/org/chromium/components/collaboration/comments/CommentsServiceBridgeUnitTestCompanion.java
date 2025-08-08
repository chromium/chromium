// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.collaboration.comments;

import android.os.Looper;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.mockito.ArgumentCaptor;
import org.mockito.Mockito;

import org.chromium.base.Callback;
import org.chromium.base.ThreadUtils;
import org.chromium.url.GURL;

import java.util.UUID;

/** A companion object to the native CommentsServiceBridgeTest. */
@JNINamespace("collaboration::comments::android")
public class CommentsServiceBridgeUnitTestCompanion {
    private final CommentsService mService;
    private final Callback<Boolean> mSuccessCallback = Mockito.mock(Callback.class);
    private final ArgumentCaptor<Boolean> mCallbackValueCaptor =
            ArgumentCaptor.forClass(Boolean.class);

    @CalledByNative
    private CommentsServiceBridgeUnitTestCompanion(CommentsService service) {
        mService = service;
        ThreadUtils.setUiThread(Looper.getMainLooper());
    }

    @CalledByNative
    private boolean isInitialized() {
        return mService.isInitialized();
    }

    @CalledByNative
    private boolean isEmptyService() {
        return mService.isEmptyService();
    }

    @CalledByNative
    private String addComment(
            String collaborationId,
            GURL url,
            String content,
            String parentCommentId,
            Callback<Boolean> callback) {
        return mService.addComment(
                        collaborationId, url, content, UUID.fromString(parentCommentId), callback)
                .toString();
    }

    @CalledByNative
    private void editComment(String commentId, String newContent, Callback<Boolean> callback) {
        mService.editComment(UUID.fromString(commentId), newContent, callback);
    }

    @CalledByNative
    private void deleteComment(String commentId, Callback<Boolean> callback) {
        mService.deleteComment(UUID.fromString(commentId), callback);
    }

    @CalledByNative
    private Callback<Boolean> getBooleanCallback() {
        return mSuccessCallback;
    }

    @CalledByNative
    private void verifyBooleanCallback(boolean expectedValue) {
        Mockito.verify(mSuccessCallback, Mockito.times(1)).onResult(mCallbackValueCaptor.capture());
        assert mCallbackValueCaptor.getValue().equals(expectedValue);
    }
}
