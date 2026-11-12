// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.accessibility;

import android.content.Context;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.components.browser_ui.settings.SettingsNavigation;
import org.chromium.components.web_distiller.core.DistilledPagePrefs;
import org.chromium.content_public.browser.BrowserContextHandle;

/**
 * An interface implemented by the embedder that allows the Accessibility Settings UI to access
 * embedder-specific logic.
 */
@NullMarked
public interface AccessibilitySettingsDelegate {
    /** An interface to control a single integer preference. */
    interface IntegerPreferenceDelegate {
        /** Returns the current value of the preference of this instance. */
        int getValue();

        /** Sets a new value for the preference of this instance. */
        void setValue(int value);
    }

    /** An interface to control a single boolean preference. */
    interface BooleanPreferenceDelegate {
        /** Returns the current value of the preference of this instance. */
        boolean getValue();

        /** Sets a new value for the preference of this instance. */
        void setValue(boolean value);
    }

    /** Returns the BrowserContextHandle that should be used to read and update settings. */
    BrowserContextHandle getBrowserContextHandle();

    /** Returns whether the Image Descriptions user setting should be shown. */
    boolean shouldShowImageDescriptionsSetting();

    /** Returns the SettingsNavigation for navigating between Settings pages. */
    SettingsNavigation getSiteSettingsNavigation();

    /**
     * Returns the SettingsNavigation for navigating between Settings pages.
     *
     * @param context Android context used for scoping settings navigation in tab mode.
     */
    default SettingsNavigation getSiteSettingsNavigation(Context context) {
        return getSiteSettingsNavigation();
    }

    /**
     * Returns the IntegerPreferenceDelegate instance that should be used for reading and setting
     * the text size contrast value for accessibility settings. Return null to omit the preference.
     */
    IntegerPreferenceDelegate getTextSizeContrastAccessibilityDelegate();

    /**
     * Returns the BooleanPreferenceDelegate instance that should be used for reading and setting
     * the force enable zoom value for accessibility settings. Return null to omit the preference.
     */
    BooleanPreferenceDelegate getForceEnableZoomAccessibilityDelegate();

    /**
     * Returns the BooleanPreferenceDelegate instance that should be used for reading and setting
     * the touchpad overscroll history navigation value for accessibility settings. Return null to
     * omit the preference.
     */
    BooleanPreferenceDelegate getTouchpadOverscrollHistoryNavigationAccessibilityDelegate();

    /**
     * Returns the BooleanPreferenceDelegate instance that should be used for reading and setting
     * the reader (simplified view) value for accessibility settings. Return null to omit the
     * preference.
     */
    BooleanPreferenceDelegate getReaderAccessibilityDelegate();

    /** Returns an instance of DistilledPagePrefs. */
    DistilledPagePrefs getDistilledPagePrefs();

    /** Returns whether caret browsing is enabled. */
    boolean isCaretBrowsingEnabled();

    /** Sets whether caret browsing is enabled. */
    void setCaretBrowsingEnabled(boolean enabled);

    /** Returns the String preference key for caret browsing. */
    default @Nullable String getCaretBrowsingPreferenceKey() {
        return null;
    }
}
