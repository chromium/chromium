// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.widget.chips;

import androidx.annotation.DrawableRes;
import androidx.annotation.Nullable;
import androidx.annotation.StringRes;

import org.chromium.ui.widget.ChipView;

/**
 * A generic visual representation of a Chip. Most of the visuals are immutable, but the selection
 * and enable states are not.
 */
public class Chip {
    /** An id to use for {@link #icon} when there is no icon on the chip. */
    public static final int INVALID_ICON_ID = ChipView.INVALID_ICON_ID;

    /** An id to use for the StringRes when we don't have a valid ID. */
    public static final int INVALID_STRING_RES_ID = -1;

    /** An id used to identify this chip. */
    public final int id;

    /**
     * The resource id for the text to show in the chip, or {@link #INVALID_STRING_RES_ID} when the
     * text was supplied through the constructor that takes {@link #rawText}.
     */
    public final @StringRes int text;

    /**
     * The raw text to show in the chip, or {@code null} when the text was supplied as a resource in
     * the constructor that takes {@link #text}.
     */
    @Nullable
    public final String rawText;

    /** The accessibility text to use for the chip. */
    public String contentDescription;

    /** The resource id for the icon to use in the chip. */
    public final @DrawableRes int icon;

    /** The {@link Runnable} to trigger when this chip is selected by the UI. */
    public final Runnable chipSelectedListener;

    /** Whether or not this Chip is enabled. */
    public boolean enabled;

    /** Whether or not this Chip is selected. */
    public boolean selected;

    /**
     * Builds a new {@link Chip} instance.  These properties cannot be changed.
     * @param id An arbitrary integer identifier to associate with this Chip.
     * @param text A resource ID of the string to show inside the Chip.
     * @param icon A resource ID of a drawable to show as an icon inside the Chip.
     * @param id A {@link Runnable} to call when the user taps on the Chip.
     */
    public Chip(int id, @StringRes int text, @DrawableRes int icon, Runnable chipSelectedListener) {
        this.id = id;
        this.text = text;
        this.rawText = null;
        this.icon = icon;
        this.chipSelectedListener = chipSelectedListener;
    }

    /**
     * Builds a new {@link Chip} instance. These properties cannot be changed.
     * @param id An arbitrary integer identifier to associate with this Chip.
     * @param rawText The raw text string to show inside the Chip.
     * @param icon A resource ID of a drawable to show as an icon inside the Chip.
     * @param id A {@link Runnable} to call when the user taps on the Chip.
     */
    public Chip(int id, String rawText, @DrawableRes int icon, Runnable chipSelectedListener) {
        this.id = id;
        this.rawText = rawText;
        this.text = INVALID_STRING_RES_ID;
        this.icon = icon;
        this.chipSelectedListener = chipSelectedListener;
    }
}
