// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.media_router.caf;

import android.content.Intent;

import org.chromium.components.browser_ui.media.MediaNotificationUma;
import org.chromium.components.media_router.R;

/** NotificationController implementation for presentation. */
public class CafNotificationController extends BaseNotificationController {
    public CafNotificationController(BaseSessionController sessionController) {
        super(sessionController);
        sessionController.addCallback(this);
    }

    @Override
    public Intent createContentIntent() {
        Intent contentIntent = createBringTabToFrontIntent();
        if (contentIntent != null) {
            contentIntent.putExtra(MediaNotificationUma.INTENT_EXTRA_NAME,
                    MediaNotificationUma.Source.PRESENTATION);
        }
        return contentIntent;
    }

    @Override
    public int getNotificationId() {
        return R.id.presentation_notification;
    }
}
