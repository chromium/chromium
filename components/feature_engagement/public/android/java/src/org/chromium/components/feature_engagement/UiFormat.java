// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.feature_engagement;

import androidx.annotation.IntDef;

import org.chromium.build.annotations.NullMarked;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/** UI format types for in-product help and user education surfaces on Android. */
@IntDef({
    UiFormat.UNKNOWN,
    UiFormat.BLUE_BUBBLE,
    UiFormat.BOTTOM_SHEET,
    UiFormat.FRE,
    UiFormat.SET_UP_LIST,
    UiFormat.ALERTS,
    UiFormat.MAGIC_STACK,
    UiFormat.NOTIFICATIONS,
    UiFormat.MESSAGES,
    UiFormat.INLINE_PROMO,
    UiFormat.DIALOG,
})
@Retention(RetentionPolicy.SOURCE)
@NullMarked
public @interface UiFormat {
    int UNKNOWN = 0;
    int BLUE_BUBBLE = 1;
    int BOTTOM_SHEET = 2;
    int FRE = 3;
    int SET_UP_LIST = 4;
    int ALERTS = 5;
    int MAGIC_STACK = 6;
    int NOTIFICATIONS = 7;
    int MESSAGES = 8;
    int INLINE_PROMO = 9;
    int DIALOG = 10;
}
