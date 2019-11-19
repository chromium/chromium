// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.util;

import android.view.View;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.infobar.InfoBar;
import org.chromium.content_public.browser.test.util.Criteria;
import org.chromium.content_public.browser.test.util.CriteriaHelper;
import org.chromium.content_public.browser.test.util.TestThreadUtils;

import java.util.List;

/**
 * Utility functions for dealing with InfoBars.
 */
public class InfoBarUtil {
    /**
     * Finds, and optionally clicks, the button with the specified ID in the given InfoBar.
     * @return True if the View was found.
     */
    public static boolean findButton(InfoBar infoBar, int buttonId, boolean click) {
        final View button = infoBar.getView().findViewById(buttonId);
        if (button == null) return false;
        if (click) {
            TestThreadUtils.runOnUiThreadBlocking(() -> { button.performClick(); });
        }
        return true;
    }

    /**
     * Checks if the primary button exists on the InfoBar.
     * @return True if the View was found.
     */
    public static boolean hasPrimaryButton(InfoBar infoBar) {
        return findButton(infoBar, R.id.button_primary, false);
    }

    /**
     * Checks if the secondary button exists on the InfoBar.
     * @return True if the View was found.
     */
    public static boolean hasSecondaryButton(InfoBar infoBar) {
        return findButton(infoBar, R.id.button_secondary, false);
    }

    /**
     * Simulates clicking the Close button in the specified infobar.
     * @return True if the View was found.
     */
    public static boolean clickCloseButton(InfoBar infoBar) {
        return findButton(infoBar, R.id.infobar_close_button, true);
    }

    /**
     * Simulates clicking the primary button in the specified infobar.
     * @return True if the View was found.
     */
    public static boolean clickPrimaryButton(InfoBar infoBar) {
        return findButton(infoBar, R.id.button_primary, true);
    }

    /**
     * Simulates clicking the secondary button in the specified infobar.
     * @return True if the View was found.
     */
    public static boolean clickSecondaryButton(InfoBar infoBar) {
        return findButton(infoBar, R.id.button_secondary, true);
    }

    /**
     * Waits until the specified InfoBar list contains no info bars.
     */
    public static void waitUntilNoInfoBarsExist(final List<InfoBar> infoBars) {
        CriteriaHelper.pollUiThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return infoBars.isEmpty();
            }
        });
    }
}
