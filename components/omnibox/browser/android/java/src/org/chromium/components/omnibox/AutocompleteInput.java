// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.omnibox;

import android.text.TextUtils;

import org.chromium.build.annotations.Initializer;
import org.chromium.build.annotations.NullMarked;
import org.chromium.components.metrics.OmniboxEventProtos.OmniboxEventProto.PageClassification;
import org.chromium.url.GURL;

/** AutocompleteInput encompasses the input to autocomplete. */
@NullMarked
public class AutocompleteInput {
    private GURL mPageUrl;
    private int mPageClassification;
    private String mUserText;
    private boolean mAllowExactKeywordMatch;

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

    /** Set the page URL for the input. */
    public void setPageUrl(GURL pageUrl) {
        mPageUrl = pageUrl;
    }

    /** Returns the current page URL. */
    public GURL getPageUrl() {
        return mPageUrl;
    }

    /** Set the text as currently typed by the User. */
    public void setUserText(String text) {
        boolean oldTextUsesKeywordActivator =
                !TextUtils.isEmpty(mUserText) && TextUtils.indexOf(mUserText, ' ') > 0;
        boolean newTextUsesKeywordActivator =
                !TextUtils.isEmpty(text) && TextUtils.indexOf(text, ' ') > 0;

        // Allow engaging Keyword mode only if the user input introduces first space.
        mAllowExactKeywordMatch |= !oldTextUsesKeywordActivator && newTextUsesKeywordActivator;
        // Suppress Keyword mode when reverting back to the url.
        mAllowExactKeywordMatch &= !(oldTextUsesKeywordActivator && !newTextUsesKeywordActivator);

        mUserText = text;
    }

    /** Returns whether exact keyword match is allowed with current input. */
    public boolean allowExactKeywordMatch() {
        return mAllowExactKeywordMatch;
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
        mAllowExactKeywordMatch = false;
        mPageUrl = GURL.emptyGURL();
        mPageClassification = PageClassification.BLANK_VALUE;
    }
}
