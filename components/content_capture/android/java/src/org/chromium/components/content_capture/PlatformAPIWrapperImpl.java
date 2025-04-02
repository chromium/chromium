// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.content_capture;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.content.LocusId;
import android.os.Build;
import android.os.Bundle;
import android.view.ViewStructure;
import android.view.autofill.AutofillId;
import android.view.contentcapture.ContentCaptureContext;
import android.view.contentcapture.ContentCaptureSession;

import androidx.annotation.RequiresApi;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

/** The implementation of PlatformAPIWrapper. */
@RequiresApi(Build.VERSION_CODES.Q)
@NullMarked
public class PlatformAPIWrapperImpl extends PlatformAPIWrapper {
    @Override
    public ContentCaptureSession createContentCaptureSession(
            ContentCaptureSession parent, String url, @Nullable String favicon) {
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
    public void notifyFaviconUpdated(ContentCaptureSession session, @Nullable String favicon) {
        Bundle bundle = new Bundle();
        if (favicon != null) bundle.putCharSequence("favicon", favicon);
        LocusId locusId = assumeNonNull(session.getContentCaptureContext()).getLocusId();
        assert locusId != null;
        session.setContentCaptureContext(
                new ContentCaptureContext.Builder(locusId).setExtras(bundle).build());
    }

    @Override
    public void flush(ContentCaptureSession session) {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.BAKLAVA) {
            session.flush();
        }
    }
}
