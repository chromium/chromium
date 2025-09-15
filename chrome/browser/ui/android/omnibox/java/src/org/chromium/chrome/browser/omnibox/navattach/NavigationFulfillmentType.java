// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.navattach;

import androidx.annotation.IntDef;

import org.chromium.build.annotations.NullMarked;

import java.lang.annotation.ElementType;
import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.lang.annotation.Target;

/** The type of fulfillment for the navigation. */
@IntDef({NavigationFulfillmentType.DEFAULT, NavigationFulfillmentType.AI_MODE})
@Retention(RetentionPolicy.SOURCE)
@Target({ElementType.TYPE_USE})
@NullMarked
public @interface NavigationFulfillmentType {
    /** Standard search fulfillment. */
    int DEFAULT = 0;

    /** AI-powered fulfillment. */
    int AI_MODE = 1;
}
