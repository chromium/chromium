// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browserservices;

import android.app.Notification;
import android.support.customtabs.trusted.TrustedWebActivityService;

/**
 * A TrustedWebActivityService to be used in TrustedWebActivityClientTest.
 */
public class TestTrustedWebActivityService extends TrustedWebActivityService {
    // TODO(peconn): Add an image resource to chrome_public_test_support, supply that in
    // getSmallIconId and verify it is used in notifyNotificationWithChannel.
    public static final int SMALL_ICON_ID = 42;

    @Override
    public void onCreate() {
        super.onCreate();
        TrustedWebActivityService.setVerifiedProviderForTesting(this, "org.chromium.chrome");
    }

    @Override
    protected boolean notifyNotificationWithChannel(String platformTag, int platformId,
            Notification notification, String channelName) {
        MessengerService.sMessageHandler
                .recordNotifyNotification(platformTag, platformId, channelName);
        return true;
    }

    @Override
    protected void cancelNotification(String platformTag, int platformId) {
        MessengerService.sMessageHandler.recordCancelNotification(platformTag, platformId);
    }

    @Override
    protected int getSmallIconId() {
        MessengerService.sMessageHandler.recordGetSmallIconId();
        return SMALL_ICON_ID;
    }
}
