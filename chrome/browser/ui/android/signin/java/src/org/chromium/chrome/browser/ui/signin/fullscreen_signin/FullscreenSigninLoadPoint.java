// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.signin.fullscreen_signin;

import androidx.annotation.IntDef;

import org.chromium.build.annotations.NullMarked;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

// LINT.IfChange(LoadPoint)
/**
 * The input that completed the fullscreen sign-in screen's initial load, i.e. the last one to
 * arrive. Recorded in the MobileFre.SlowestLoadPoint histogram, so entries should be treated as
 * append-only.
 */
@IntDef({
    FullscreenSigninLoadPoint.NATIVE_INITIALIZATION,
    FullscreenSigninLoadPoint.POLICY_LOAD,
    FullscreenSigninLoadPoint.CHILD_STATUS_LOAD,
    FullscreenSigninLoadPoint.ACCOUNT_FETCHING,
    FullscreenSigninLoadPoint.MAX
})
@Retention(RetentionPolicy.SOURCE)
@NullMarked
public @interface FullscreenSigninLoadPoint {
    int NATIVE_INITIALIZATION = 0;
    int POLICY_LOAD = 1;
    int CHILD_STATUS_LOAD = 2;
    int ACCOUNT_FETCHING = 3;
    int MAX = 4;
}
// LINT.ThenChange(//tools/metrics/histograms/metadata/mobile/enums.xml:LoadPoint)
