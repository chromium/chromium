// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.infobars;

import android.view.View;

import androidx.annotation.IntDef;

import org.chromium.chrome.browser.infobar.InfoBarIdentifier;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/** An interface for items that can be added to an InfoBarContainerLayout. */
public interface InfoBarUiItem {
    // The infobar priority.
    @IntDef({
        InfoBarPriority.CRITICAL,
        InfoBarPriority.USER_TRIGGERED,
        InfoBarPriority.PAGE_TRIGGERED,
        InfoBarPriority.BACKGROUND
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface InfoBarPriority {
        int CRITICAL = 0;
        int USER_TRIGGERED = 1;
        int PAGE_TRIGGERED = 2;
        int BACKGROUND = 3;
    }

    /**
     * Returns the View that represents this infobar. This should have no background or borders;
     * a background and shadow will be added by a wrapper view.
     */
    View getView();

    /**
     * Returns whether controls for this View should be clickable. If false, all input events on
     * this item will be ignored.
     */
    boolean areControlsEnabled();

    /**
     * Sets whether or not controls for this View should be clickable. This does not affect the
     * visual state of the infobar.
     * @param state If false, all input events on this Item will be ignored.
     */
    void setControlsEnabled(boolean state);

    /** Returns the accessibility text to announce when this infobar is first shown. */
    CharSequence getAccessibilityText();

    /**
     * Returns the priority of an infobar. High priority infobar is shown in front of low
     * priority infobar. If infobars have the same priorities, the most recently added one
     * is shown behind previous ones.
     *
     */
    int getPriority();

    /**
     * Returns the type of infobar, as best as can be determined at this time.  See
     * components/infobars/core/infobar_delegate.h.
     */
    @InfoBarIdentifier
    int getInfoBarIdentifier();
}
