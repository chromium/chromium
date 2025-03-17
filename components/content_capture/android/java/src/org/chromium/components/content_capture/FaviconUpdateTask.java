// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.content_capture;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.os.Build;

import androidx.annotation.RequiresApi;

import org.chromium.build.annotations.NullMarked;
import org.chromium.components.content_capture.PlatformSession.PlatformSessionData;

/** The task to update the favicon to plateform. */
@NullMarked
@RequiresApi(Build.VERSION_CODES.Q)
public class FaviconUpdateTask extends NotificationTask {
    public FaviconUpdateTask(FrameSession session, PlatformSession platformSession) {
        super(session, platformSession);
    }

    @Override
    protected void runTask() {
        updateFavicon();
    }

    private void updateFavicon() {
        log("FaviconUpdateTask.updateFavicon");
        PlatformSessionData parentPlatformSessionData = buildCurrentSession();
        if (parentPlatformSessionData == null) return;
        assumeNonNull(mSession);
        PlatformAPIWrapper.getInstance()
                .notifyFaviconUpdated(
                        parentPlatformSessionData.contentCaptureSession,
                        mSession.get(0).getFavicon());
    }
}
