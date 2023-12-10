// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.content_capture;

import android.os.Build;
import android.view.ViewStructure;
import android.view.autofill.AutofillId;
import android.view.contentcapture.ContentCaptureSession;

import androidx.annotation.RequiresApi;

import org.chromium.base.ResettersForTesting;

/**
 * The class to wrap ContentCapture platform APIs, catches the exception from platform, and
 * re-throws with PlatformAPIException, so the call sites can catch platform exception to avoid
 * the crash.
 */
@RequiresApi(Build.VERSION_CODES.Q)
public abstract class PlatformAPIWrapper {
    private static PlatformAPIWrapper sImpl;

    public static PlatformAPIWrapper getInstance() {
        if (sImpl == null) {
            sImpl = new PlatformAPIWrapperImpl();
        }
        return sImpl;
    }

    public abstract ContentCaptureSession createContentCaptureSession(
            ContentCaptureSession parent, String url, String favicon);

    public abstract void destroyContentCaptureSession(ContentCaptureSession session);

    public abstract AutofillId newAutofillId(
            ContentCaptureSession parent, AutofillId rootAutofillId, long id);

    public abstract ViewStructure newVirtualViewStructure(
            ContentCaptureSession parent, AutofillId parentAutofillId, long id);

    public abstract void notifyViewAppeared(
            ContentCaptureSession session, ViewStructure viewStructure);

    public abstract void notifyViewDisappeared(
            ContentCaptureSession session, AutofillId autofillId);

    public abstract void notifyViewsDisappeared(
            ContentCaptureSession session, AutofillId autofillId, long[] ids);

    public abstract void notifyViewTextChanged(
            ContentCaptureSession session, AutofillId autofillId, String newContent);

    public abstract void notifyFaviconUpdated(ContentCaptureSession session, String favicon);

    public static void setPlatformAPIWrapperImplForTesting(PlatformAPIWrapper impl) {
        var oldValue = sImpl;
        sImpl = impl;
        ResettersForTesting.register(() -> sImpl = oldValue);
    }
}
