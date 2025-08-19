// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.settings;

import androidx.annotation.IntDef;

import org.chromium.build.annotations.NullMarked;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/** An interface for preferences that should have custom background styling applied to them. */
@NullMarked
public interface CustomStyledPreference {
    @IntDef({BackgroundStyle.NONE})
    @Retention(RetentionPolicy.SOURCE)
    @interface BackgroundStyle {
        int NONE = 0;
    }

    /** Returns the custom background style for the preference. */
    @BackgroundStyle
    int getCustomBackgroundStyle();
}
