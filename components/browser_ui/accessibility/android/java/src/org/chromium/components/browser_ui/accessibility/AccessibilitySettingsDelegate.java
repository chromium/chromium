// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.accessibility;

import org.chromium.build.annotations.NullMarked;
import org.chromium.components.browser_ui.settings.SettingsNavigation;
import org.chromium.content_public.browser.BrowserContextHandle;

/**
 * An interface implemented by the embedder that allows the Accessibility Settings UI to access
 * embedder-specific logic.
 */
@NullMarked
public interface AccessibilitySettingsDelegate {
    /** An interface to control a single integer preference. */
    interface IntegerPreferenceDelegate {
        /**
         * @return int - Current value of the preference of this instance.
         */
        int getValue();

        /** Sets a new value for the preference of this instance. */
        void setValue(int value);
    }

    /** An interface to control a single integer preference. */
    interface BooleanPreferenceDelegate {
        /**
         * @return boolean - Current value of the preference of this instance.
         */
        boolean getValue();

        /** Sets a new value for the preference of this instance. */
        void setValue(boolean value);
    }

    /**
     * @return The BrowserContextHandle that should be used to read and update settings.
     */
    BrowserContextHandle getBrowserContextHandle();

    /**
     * @return boolean value specifying if the Image Descriptions user setting should be shown.
     */
    boolean shouldShowImageDescriptionsSetting();

    /**
     * @return SettingsNavigation for navigating between Settings pages.
     */
    SettingsNavigation getSiteSettingsNavigation();

    /**
     * @return the InterPreferenceDelegate instance that should be used for reading and setting the
     *     text size contrast value for accessibility settings. Return null to omit the preference.
     */
    IntegerPreferenceDelegate getTextSizeContrastAccessibilityDelegate();

    /**
     * @return the BooleanPreferenceDelegate instance that should be used for reading and setting
     *     the force enable zoom value for accessibility settings. Return null to omit the
     *     preference.
     */
    BooleanPreferenceDelegate getForceEnableZoomAccessibilityDelegate();

    /**
     * @return the BooleanPreferenceDelegate instance that should be used for reading and setting
     *     the touchpad overscroll history navigation value for accessibility settings. Return null
     *     to omit the preference.
     */
    BooleanPreferenceDelegate getTouchpadOverscrollHistoryNavigationAccessibilityDelegate();

    /**
     * @return the BooleanPreferenceDelegate instance that should be used for reading and setting
     *     the reader (simplified view) value for accessibility settings. Return null to omit the
     *     preference.
     */
    BooleanPreferenceDelegate getReaderAccessibilityDelegate();

    /**
     * Returns whether the material slider should be used for the page zoom preference.
     *
     * @return True if the slider should be used, false otherwise.
     */
    boolean shouldUseSlider();

    /**
     * Returns whether caret browsing is enabled.
     *
     * @return boolean - Whether caret browsing is enabled.
     */
    boolean isCaretBrowsingEnabled();

    /** Sets whether caret browsing is enabled. */
    void setCaretBrowsingEnabled(boolean enabled);
}
