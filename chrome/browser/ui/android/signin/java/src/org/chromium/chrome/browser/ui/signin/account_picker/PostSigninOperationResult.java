// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.signin.account_picker;

import androidx.annotation.IntDef;

import org.chromium.build.annotations.NullMarked;

import java.lang.annotation.ElementType;
import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.lang.annotation.Target;

/** Result of the post-sign-in operation. */
@IntDef({
    PostSigninOperationResult.SUCCESS,
    PostSigninOperationResult.AUTH_ERROR,
    PostSigninOperationResult.OTHER_ERROR
})
@Retention(RetentionPolicy.SOURCE)
@Target({ElementType.TYPE_USE})
@NullMarked
public @interface PostSigninOperationResult {
    /** The operation succeeded; sign-in flow should continue (e.g. to History Sync). */
    int SUCCESS = 0;

    /** An authentication error occurred; re-authentication screen should be shown. */
    int AUTH_ERROR = 1;

    /** A generic error occurred; error screen with a retry option should be shown. */
    int OTHER_ERROR = 2;
}
