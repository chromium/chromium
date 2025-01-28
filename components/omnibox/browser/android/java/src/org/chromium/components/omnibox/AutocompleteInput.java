// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.omnibox;

import android.text.TextUtils;

import org.chromium.build.annotations.Initializer;
import org.chromium.build.annotations.NullMarked;
import org.chromium.components.metrics.OmniboxEventProtos.OmniboxEventProto.PageClassification;

/** AutocompleteInput encompasses the input to autocomplete. */
@NullMarked
public class AutocompleteInput {
    private int mPageClassification;
    private String mUserText;

    public AutocompleteInput() {
        reset();
    }

    /** Set the PageClassification for the input. */
    public void setPageClassification(int pageClassification) {
        mPageClassification = pageClassification;
    }

    /** Returns the current page classification. */
    public int getPageClassification() {
        return mPageClassification;
    }

    /** Set the text as currently typed by the User. */
    public void setUserText(String text) {
        mUserText = text;
    }

    /** Returns the text as currently typed by the User. */
    public String getUserText() {
        return mUserText;
    }

    /** Returns whether current context represents zero-prefix context. */
    public boolean isInZeroPrefixContext() {
        return TextUtils.isEmpty(mUserText);
    }

    /** Returns whether current context enables suggestions caching. */
    public boolean isInCacheableContext() {
        if (!isInZeroPrefixContext()) return false;

        switch (mPageClassification) {
            case PageClassification.ANDROID_SEARCH_WIDGET_VALUE:
            case PageClassification.ANDROID_SHORTCUTS_WIDGET_VALUE:
                return true;

            case PageClassification.INSTANT_NTP_WITH_OMNIBOX_AS_STARTING_FOCUS_VALUE:
                return OmniboxFeatures.isJumpStartOmniboxEnabled();

            case PageClassification.SEARCH_RESULT_PAGE_NO_SEARCH_TERM_REPLACEMENT_VALUE:
            case PageClassification.OTHER_VALUE:
                return OmniboxFeatures.sJumpStartOmniboxCoverRecentlyVisitedPage.getValue();

            default:
                return false;
        }
    }

    @Initializer
    public void reset() {
        mUserText = "";
    }
}
