// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.settings;

import android.os.Bundle;
import android.view.View;

import androidx.core.view.AccessibilityDelegateCompat;
import androidx.core.view.ViewCompat;
import androidx.core.view.accessibility.AccessibilityNodeInfoCompat;
import androidx.core.view.accessibility.AccessibilityNodeInfoCompat.AccessibilityActionCompat;
import androidx.preference.Preference;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

import java.util.function.BooleanSupplier;

/**
 * A reusable accessibility delegate for expandable preferences. It manages the expanded state and
 * exposes expand/collapse actions to accessibility services.
 */
@NullMarked
public class ExpandablePreferenceAccessibilityDelegate extends AccessibilityDelegateCompat {
    private final Preference mPreference;
    private final BooleanSupplier mExpandedSupplier;

    public ExpandablePreferenceAccessibilityDelegate(
            Preference preference, BooleanSupplier expandedSupplier) {
        mPreference = preference;
        mExpandedSupplier = expandedSupplier;
    }

    @Override
    public void onInitializeAccessibilityNodeInfo(View host, AccessibilityNodeInfoCompat info) {
        super.onInitializeAccessibilityNodeInfo(host, info);
        boolean expanded = mExpandedSupplier.getAsBoolean();
        info.setExpandedState(
                expanded
                        ? AccessibilityNodeInfoCompat.EXPANDED_STATE_FULL
                        : AccessibilityNodeInfoCompat.EXPANDED_STATE_COLLAPSED);
        info.addAction(
                expanded
                        ? AccessibilityActionCompat.ACTION_COLLAPSE
                        : AccessibilityActionCompat.ACTION_EXPAND);
    }

    @Override
    public boolean performAccessibilityAction(View host, int action, @Nullable Bundle arguments) {
        if (action == AccessibilityActionCompat.ACTION_EXPAND.getId()
                || action == AccessibilityActionCompat.ACTION_COLLAPSE.getId()) {
            mPreference.performClick();
            return true;
        }
        return super.performAccessibilityAction(host, action, arguments);
    }

    /** Helper to apply this delegate to both the container and title view. */
    public static void apply(
            Preference preference,
            View container,
            @Nullable View title,
            BooleanSupplier expandedSupplier) {
        ExpandablePreferenceAccessibilityDelegate delegate =
                new ExpandablePreferenceAccessibilityDelegate(preference, expandedSupplier);
        ViewCompat.setAccessibilityDelegate(container, delegate);
        if (title != null) {
            ViewCompat.setAccessibilityDelegate(title, delegate);
        }
    }
}
