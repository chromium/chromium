// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.media_router.caf.remoting;

import android.content.Intent;

import org.chromium.base.ContextUtils;
import org.chromium.components.browser_ui.media.MediaNotificationUma;
import org.chromium.components.media_router.MediaRouterClient;
import org.chromium.components.media_router.caf.BaseNotificationController;
import org.chromium.components.media_router.caf.BaseSessionController;

/** NotificationController implementation for remoting. */
public class RemotingNotificationController extends BaseNotificationController {
    public RemotingNotificationController(BaseSessionController sessionController) {
        super(sessionController);
        sessionController.addCallback(this);
    }

    @Override
    public Intent createContentIntent() {
        Intent contentIntent = new Intent(
                ContextUtils.getApplicationContext(), CafExpandedControllerActivity.class);
        contentIntent.putExtra(
                MediaNotificationUma.INTENT_EXTRA_NAME, MediaNotificationUma.Source.MEDIA_FLING);
        return contentIntent;
    }

    @Override
    public int getNotificationId() {
        return MediaRouterClient.getInstance().getRemotingNotificationId();
    }
}
