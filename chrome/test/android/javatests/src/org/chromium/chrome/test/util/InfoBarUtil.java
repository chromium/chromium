// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.util;

import android.view.View;

import org.hamcrest.BaseMatcher;
import org.hamcrest.Description;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.infobar.InfoBarIdentifier;
import org.chromium.components.infobars.InfoBar;

import java.util.List;

/** Utility functions for dealing with InfoBars. */
public class InfoBarUtil {
    /**
     * Finds, and optionally clicks, the button with the specified ID in the given InfoBar.
     * @return True if the View was found.
     */
    public static boolean findButton(InfoBar infoBar, int buttonId, boolean click) {
        final View button = infoBar.getView().findViewById(buttonId);
        if (button == null) return false;
        if (click) {
            ThreadUtils.runOnUiThreadBlocking(
                    () -> {
                        button.performClick();
                    });
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

    /** Waits until the specified InfoBar list contains no info bars. */
    public static void waitUntilNoInfoBarsExist(final List<InfoBar> infoBars) {
        CriteriaHelper.pollUiThread(infoBars::isEmpty);
    }

    /**
     * Matcher used to find a specific infobar (by id) and tolerant of multiple InfoBars
     * simultaneously showing.
     */
    public static class InfoBarMatcher extends BaseMatcher<InfoBar> {
        private @InfoBarIdentifier int mId;
        public InfoBar mLastMatch;

        public InfoBarMatcher(@InfoBarIdentifier int id) {
            mId = id;
        }

        @Override
        public boolean matches(Object o) {
            mLastMatch = (InfoBar) o;
            return mLastMatch.getInfoBarIdentifier() == mId;
        }

        @Override
        public void describeTo(Description description) {
            description.appendText("Couldn't find infobar with id " + mId);
        }
    }
}
