// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.content_capture;

import android.content.LocusId;
import android.os.Build;
import android.os.Bundle;
import android.view.ViewStructure;
import android.view.autofill.AutofillId;
import android.view.contentcapture.ContentCaptureContext;
import android.view.contentcapture.ContentCaptureSession;

import androidx.annotation.RequiresApi;

/** The implementation of PlatformAPIWrapper. */
@RequiresApi(Build.VERSION_CODES.Q)
public class PlatformAPIWrapperImpl extends PlatformAPIWrapper {
    @Override
    public ContentCaptureSession createContentCaptureSession(
            ContentCaptureSession parent, String url, String favicon) {
        Bundle bundle = new Bundle();
        if (favicon != null) bundle.putCharSequence("favicon", favicon);
        return parent.createContentCaptureSession(
                new ContentCaptureContext.Builder(new LocusId(url)).setExtras(bundle).build());
    }

    @Override
    public void destroyContentCaptureSession(ContentCaptureSession session) {
        session.destroy();
    }

    @Override
    public AutofillId newAutofillId(
            ContentCaptureSession parent, AutofillId rootAutofillId, long id) {
        return parent.newAutofillId(rootAutofillId, id);
    }

    @Override
    public ViewStructure newVirtualViewStructure(
            ContentCaptureSession parent, AutofillId parentAutofillId, long id) {
        return parent.newVirtualViewStructure(parentAutofillId, id);
    }

    @Override
    public void notifyViewAppeared(ContentCaptureSession session, ViewStructure viewStructure) {
        session.notifyViewAppeared(viewStructure);
    }

    @Override
    public void notifyViewDisappeared(ContentCaptureSession session, AutofillId autofillId) {
        session.notifyViewDisappeared(autofillId);
    }

    @Override
    public void notifyViewsDisappeared(
            ContentCaptureSession session, AutofillId autofillId, long[] ids) {
        session.notifyViewsDisappeared(autofillId, ids);
    }

    @Override
    public void notifyViewTextChanged(
            ContentCaptureSession session, AutofillId autofillId, String newContent) {
        session.notifyViewTextChanged(autofillId, newContent);
    }

    @Override
    public void notifyFaviconUpdated(ContentCaptureSession session, String favicon) {
        Bundle bundle = new Bundle();
        if (favicon != null) bundle.putCharSequence("favicon", favicon);
        session.setContentCaptureContext(
                new ContentCaptureContext.Builder(session.getContentCaptureContext().getLocusId())
                        .setExtras(bundle)
                        .build());
    }
}
