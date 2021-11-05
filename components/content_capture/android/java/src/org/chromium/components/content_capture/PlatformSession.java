// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.content_capture;

import android.annotation.TargetApi;
import android.os.Build;
import android.view.View;
import android.view.ViewStructure;
import android.view.autofill.AutofillId;
import android.view.contentcapture.ContentCaptureSession;
import android.widget.Checkable;

import org.chromium.base.annotations.VerifiesOnQ;

import java.util.HashMap;

/**
 * The class to manage the platform session.
 */
@VerifiesOnQ
@TargetApi(Build.VERSION_CODES.Q)
class PlatformSession {
    /**
     * PlatformSessionData wraps the ContentCaptureSession and its corresponding
     * AutofillId. The AutofillId should be consistent with Android Autofill's
     * AutofillId, so the service can use content capture as the heuristic for
     * autofill.
     */
    public static final class PlatformSessionData {
        public final ContentCaptureSession contentCaptureSession;
        public final AutofillId autofillId;

        public PlatformSessionData(ContentCaptureSession session, AutofillId id) {
            contentCaptureSession = session;
            autofillId = id;
        }
    }

    private PlatformSessionData mRootPlatformSessionData;
    private HashMap<Long, PlatformSessionData> mFrameIdToPlatformSessionData;

    public static PlatformSession fromView(View view) {
        ContentCaptureSession session = view.getContentCaptureSession();
        if (session == null) return null;
        ViewStructure structure = session.newViewStructure(view);
        AutofillId autofillId = structure.getAutofillId();
        if (autofillId == null) return null;
        // Simulate the logical in View.onProvideStructure()
        structure.setDimens(view.getLeft(), view.getTop(), 0, 0, view.getRight() - view.getLeft(),
                view.getBottom() - view.getTop());
        structure.setVisibility(view.getVisibility());
        structure.setEnabled(view.isEnabled());
        structure.setClickable(view.isClickable());
        structure.setFocusable(view.isFocusable());
        structure.setFocused(view.isFocused());
        structure.setAccessibilityFocused(view.isAccessibilityFocused());
        structure.setSelected(view.isSelected());
        structure.setActivated(view.isActivated());
        structure.setLongClickable(view.isLongClickable());
        if (view instanceof Checkable) {
            structure.setCheckable(true);
            if (((Checkable) view).isChecked()) structure.setChecked(true);
        }
        if (view.isOpaque()) structure.setOpaque(true);
        if (view.isContextClickable()) structure.setContextClickable(true);
        CharSequence className = view.getAccessibilityClassName();
        if (className != null) structure.setClassName(className.toString());
        structure.setContentDescription(view.getContentDescription());

        return new PlatformSession(session, autofillId);
    }

    public PlatformSession(ContentCaptureSession session, AutofillId id) {
        mRootPlatformSessionData = new PlatformSessionData(session, id);
    }

    public HashMap<Long, PlatformSessionData> getFrameIdToPlatformSessionData() {
        if (mFrameIdToPlatformSessionData == null) {
            mFrameIdToPlatformSessionData = new HashMap<Long, PlatformSessionData>();
        }
        return mFrameIdToPlatformSessionData;
    }

    public PlatformSessionData getRootPlatformSessionData() {
        return mRootPlatformSessionData;
    }
}
