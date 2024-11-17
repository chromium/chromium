// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.omnibox;

import android.text.TextUtils;

import org.chromium.components.metrics.OmniboxEventProtos.OmniboxEventProto.PageClassification;

import java.util.OptionalInt;

/** AutocompleteInput encompasses the input to autocomplete. */
public class AutocompleteInput {
    private OptionalInt mPageClassification = OptionalInt.empty();
    private String mUserText;

    /** Set the PageClassification for the input. */
    public void setPageClassification(int pageClassification) {
        mPageClassification = OptionalInt.of(pageClassification);
    }

    /** Returns the current page classification. */
    public OptionalInt getPageClassification() {
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
        if (getPageClassification().isEmpty()) return false;
        if (!isInZeroPrefixContext()) return false;

        int pageClass = getPageClassification().getAsInt();
        switch (pageClass) {
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

    public void reset() {
        mPageClassification = OptionalInt.empty();
        mUserText = null;
    }
}
