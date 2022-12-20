// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.settings;

/**
 * Interface that used to provider information about divider customization in this fragment.
 * Extending class can override any number for this class to provider information about the divider
 * customization; otherwise, the emdbedded activity will fallback using the default divider in
 * {@link androidx.preference.PreferenceFragmentCompat}.
 *
 * <pre>
 * // No customization will be used.
 * public FragmentWithDefaultDivider extends PreferenceFragmentCompat {
 * }
 *
 * // No divider will be used.
 * public FragmentWithNoDivider implements CustomDividerFragment extends PreferenceFragmentCompat {
 *     &#64;Override
 *     public boolean hasDivider() {
 *         return false;
 *     }
 * }
 *
 * // Divider will draw with paddings aside.
 * public FragmentWithDivider implements CustomDividerFragment extends PreferenceFragmentCompat{
 *     &#64;Override
 *     public boolean hasDivider() {
 *         return true;
 *     }
 *
 *     &#64;Override
 *     public int getDividerStartPadding() {
 *         return getContext().getResource().getDimension(R.dimen.divider_start_padding);
 *     }
 *
 *     &#64;Override
 *     public int getDividerEndPadding() {
 *         return getContext().getResource().getDimension(R.dimen.divider_end_padding);
 *     }
 * }
 * </pre>
 */
public interface CustomDividerFragment {
    /** Return whether divider should be added into this fragment. */
    default boolean hasDivider() {
        return true;
    }

    /** Return the padding at the start of divider. */
    default int getDividerStartPadding() {
        return 0;
    }

    /** Return the padding at the end of divider. */
    default int getDividerEndPadding() {
        return 0;
    }
}
