// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.embedder_support.contextmenu;

import androidx.annotation.DrawableRes;
import androidx.annotation.IntDef;
import androidx.annotation.Nullable;
import androidx.annotation.StringRes;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/** An object that contains the required fields to generate the context menu chip. */
public class ChipRenderParams {
    // The resource id for the chip title.
    public @StringRes int titleResourceId;

    // The resource id for the chip icon.
    public @DrawableRes int iconResourceId;

    // The callback to be called when the chip clicked.
    // A non-null ChipRenderParams will always have a non-null onClickCallback.
    public Runnable onClickCallback;

    // A callback to be called when the chip shown.
    public @Nullable Runnable onShowCallback;

    // The type of chip to render. Defined to differentiate the chip being rendered to the user
    // based on asynchronous calls.
    public @ChipType int chipType;

    // Indicates whether the chip remove icon should be hidden.
    public boolean isRemoveIconHidden;

    /** Defines the types of chips that can be rendered. */
    @IntDef({ChipType.LENS_TRANSLATE_CHIP})
    @Retention(RetentionPolicy.SOURCE)
    public @interface ChipType {
        int LENS_TRANSLATE_CHIP = 1;
    }
}
