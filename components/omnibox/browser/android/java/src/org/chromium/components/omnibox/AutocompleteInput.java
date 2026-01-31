// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.omnibox;

import android.text.TextUtils;

import org.chromium.base.UserData;
import org.chromium.build.BuildConfig;
import org.chromium.build.annotations.Initializer;
import org.chromium.build.annotations.NullMarked;
import org.chromium.components.metrics.OmniboxEventProtos.OmniboxEventProto.PageClassification;
import org.chromium.components.omnibox.AimToolsProto.ToolMode;
import org.chromium.url.GURL;

import java.util.Locale;

/**
 * AutocompleteInput encompasses the input to autocomplete and fusebox.
 *
 * <p>This class must have no dependencies on external services or logic and should be fully
 * serializable.
 */
@NullMarked
public class AutocompleteInput implements UserData {
    private GURL mPageUrl;
    private int mPageClassification;
    private String mPageTitle;
    private String mUserText;
    private boolean mAllowExactKeywordMatch;
    private boolean mHasAttachments;
    private int mSelectionStart;
    private int mSelectionEnd;
    private @AutocompleteRequestType int mRequestType;

    public AutocompleteInput() {
        reset();
    }

    /**
     * Set the PageClassification for the input.
     *
     * @param pageClassification The page classification to be used for this input.
     * @return The AutocompleteInput object.
     */
    public AutocompleteInput setPageClassification(int pageClassification) {
        mPageClassification = pageClassification;
        return this;
    }

    private int getComposeboxEquivalentOfPageClassification() {
        return switch (mPageClassification) {
            // LINT.IfChange(FuseboxSupportedPageClassifications)
            case PageClassification.INSTANT_NTP_WITH_OMNIBOX_AS_STARTING_FOCUS_VALUE ->
                    PageClassification.NTP_COMPOSEBOX_VALUE;
            case PageClassification.SEARCH_RESULT_PAGE_NO_SEARCH_TERM_REPLACEMENT_VALUE ->
                    PageClassification.SRP_OMNIBOX_COMPOSEBOX_VALUE;
            case PageClassification.OTHER_VALUE ->
                    PageClassification.OTHER_OMNIBOX_COMPOSEBOX_VALUE;
            // LINT.ThenChange(/chrome/browser/ui/android/omnibox/java/src/org/chromium/chrome/browser/omnibox/fusebox/FuseboxCoordinator.java:FuseboxSupportedPageClassifications)
            default -> {
                // TODO(crbug.com/474808407): address the issue with top resumed activity change and
                // remove condition, making assertion live again.
                if (BuildConfig.ENABLE_DEBUG_LOGS) {
                    assert false
                            : String.format(
                                    Locale.ROOT,
                                    "Unrecognized page classification: %d",
                                    mPageClassification);
                }
                yield PageClassification.OTHER_OMNIBOX_COMPOSEBOX_VALUE;
            }
        };
    }

    /** Returns the current page classification. */
    public int getPageClassification() {
        return switch (mRequestType) {
            case AutocompleteRequestType.AI_MODE, AutocompleteRequestType.IMAGE_GENERATION ->
                    getComposeboxEquivalentOfPageClassification();
            default -> mPageClassification;
        };
    }

    /**
     * Set the page URL for the input.
     *
     * @param pageUrl The URL of the page the user is currently on.
     * @return The AutocompleteInput object.
     */
    public AutocompleteInput setPageUrl(GURL pageUrl) {
        mPageUrl = pageUrl;
        return this;
    }

    /** Returns the current page URL. */
    public GURL getPageUrl() {
        return mPageUrl;
    }

    /**
     * Set the page title for the input.
     *
     * @param pageTitle The title of the page the user is currently on.
     * @return The AutocompleteInput object.
     */
    public AutocompleteInput setPageTitle(String pageTitle) {
        mPageTitle = pageTitle;
        return this;
    }

    /** Returns the current page title. */
    public String getPageTitle() {
        return mPageTitle;
    }

    /** Set the AutocompleteRequestType */
    public void setRequestType(@AutocompleteRequestType int type) {
        mRequestType = type;
    }

    /** Returns the AutocompleteRequestType value. */
    public @AutocompleteRequestType int getRequestType() {
        return mRequestType;
    }

    /** Returns the Autocomplete Tool to use to fulfill the Request. */
    public /* ToolMode */ int getToolMode() {
        return switch (mRequestType) {
            case AutocompleteRequestType.IMAGE_GENERATION ->
                    mHasAttachments
                            ? ToolMode.TOOL_MODE_IMAGE_GEN_UPLOAD_VALUE
                            : ToolMode.TOOL_MODE_IMAGE_GEN_VALUE;
            default -> ToolMode.TOOL_MODE_UNSPECIFIED_VALUE;
        };
    }

    /**
     * Set the text as currently typed by the User. This also updates the state for keyword
     * matching.
     *
     * @param text The user-typed text.
     * @return The AutocompleteInput object.
     */
    public AutocompleteInput setUserText(String text) {
        boolean oldTextUsesKeywordActivator =
                !TextUtils.isEmpty(mUserText) && TextUtils.indexOf(mUserText, ' ') > 0;
        boolean newTextUsesKeywordActivator =
                !TextUtils.isEmpty(text) && TextUtils.indexOf(text, ' ') > 0;

        // Allow engaging Keyword mode only if the user input introduces first space.
        mAllowExactKeywordMatch |= !oldTextUsesKeywordActivator && newTextUsesKeywordActivator;
        // Suppress Keyword mode when reverting back to the url.
        mAllowExactKeywordMatch &= !(oldTextUsesKeywordActivator && !newTextUsesKeywordActivator);

        mUserText = text;
        return this;
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

    public void setHasAttachments(boolean hasAttachments) {
        mHasAttachments = hasAttachments;
    }

    public void setSelection(int rangeStart, int rangeEnd) {
        mSelectionStart = rangeStart;
        mSelectionEnd = rangeEnd;
    }

    public int getSelectionStart() {
        return mSelectionStart;
    }

    public int getSelectionEnd() {
        return mSelectionEnd;
    }

    /**
     * Resets the AutocompleteInput to its default state.
     *
     * @return The reset AutocompleteInput object.
     */
    @Initializer
    public AutocompleteInput reset() {
        mUserText = "";
        mAllowExactKeywordMatch = false;
        mPageUrl = GURL.emptyGURL();
        mPageTitle = "";
        mHasAttachments = false;
        mSelectionStart = 0;
        mSelectionEnd = 0;
        mPageClassification = PageClassification.BLANK_VALUE;

        return this;
    }
}
