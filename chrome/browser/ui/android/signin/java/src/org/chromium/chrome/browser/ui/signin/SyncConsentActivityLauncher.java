// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.signin;

import androidx.annotation.IntDef;

import org.chromium.components.signin.metrics.SigninAccessPoint;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/** Allows for launching {@link SyncConsentActivity} in modularized code. */
public interface SyncConsentActivityLauncher {
    @IntDef({
        SigninAccessPoint.SETTINGS,
        SigninAccessPoint.BOOKMARK_MANAGER,
        SigninAccessPoint.RECENT_TABS,
        SigninAccessPoint.SIGNIN_PROMO,
        SigninAccessPoint.NTP_FEED_TOP_PROMO,
        SigninAccessPoint.AUTOFILL_DROPDOWN,
        SigninAccessPoint.NTP_SIGNED_OUT_ICON
    })
    @Retention(RetentionPolicy.SOURCE)
    @interface AccessPoint {}
}
