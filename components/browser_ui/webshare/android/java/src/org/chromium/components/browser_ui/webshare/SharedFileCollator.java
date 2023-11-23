// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.webshare;

import org.chromium.base.Callback;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.mojo.system.MojoResult;
import org.chromium.webshare.mojom.SharedFile;

/** Initiates the share dialog when all files have been received. */
public class SharedFileCollator implements Callback<Integer> {
    private static final String WILDCARD = "*/*";

    private int mPending;
    private Callback<Boolean> mCallback;

    /**
     * Constructs a SharedFileCollator.
     *
     * @param params the share request to issue if blobs are successfully received.
     * @param callback the callback to call if any blob is not successfully received.
     */
    public SharedFileCollator(int pendingFileCount, Callback<Boolean> callback) {
        mPending = pendingFileCount;
        mCallback = callback;

        assert mPending > 0;
    }

    /**
     * Call with a MojoResult each time a blob has been received.
     *
     * @param result a MojoResult indicating if a blob was successfully received.
     */
    @Override
    public void onResult(final Integer result) {
        if (mCallback == null) return;

        if (result == MojoResult.OK && --mPending > 0) return;

        final Callback<Boolean> callback = mCallback;
        mCallback = null;

        PostTask.postTask(
                TaskTraits.UI_DEFAULT,
                () -> {
                    callback.onResult(result == MojoResult.OK);
                });
    }

    /**
     * If the files have a common type and subtype, returns type / subtype
     * Otherwise if the files have a common type, returns type / *
     * Otherwise returns * / *
     *
     * @param files an array of files being shared.
     */
    public static String commonMimeType(SharedFile[] files) {
        if (files == null || files.length == 0) return WILDCARD;
        String[] common = files[0].blob.contentType.split("/");
        if (common.length != 2) return WILDCARD;
        for (int index = 1; index < files.length; ++index) {
            String[] current = files[index].blob.contentType.split("/");
            if (current.length != 2) return WILDCARD;
            if (!current[0].equals(common[0])) return WILDCARD;
            if (!current[1].equals(common[1])) {
                common[1] = "*";
            }
        }
        return common[0] + "/" + common[1];
    }
}
