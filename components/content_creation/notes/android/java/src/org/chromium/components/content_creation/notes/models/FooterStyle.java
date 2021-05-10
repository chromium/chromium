// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.content_creation.notes.models;

import androidx.annotation.ColorInt;

/**
 * Model class for a template's text style.
 */
public class FooterStyle {
    public final @ColorInt int color;

    /** Constructor. */
    public FooterStyle(@ColorInt int color) {
        this.color = color;
    }
}
