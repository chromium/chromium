// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.omnibox;

import android.text.TextUtils;
import android.util.Range;

import androidx.annotation.IntDef;

import org.chromium.base.UserData;
import org.chromium.base.supplier.NonNullObservableSupplier;
import org.chromium.base.supplier.NullableObservableSupplier;
import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.supplier.SettableNonNullObservableSupplier;
import org.chromium.base.supplier.SettableNullableObservableSupplier;
import org.chromium.build.BuildConfig;
import org.chromium.build.annotations.Initializer;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.components.metrics.OmniboxEventProtos.OmniboxEventProto.PageClassification;
import org.chromium.components.omnibox.ToolModeProto.ToolMode;
import org.chromium.url.GURL;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.Locale;

/**
 * AutocompleteInput encompasses the input to autocomplete and fusebox.
 *
 * <p>This class must have no dependencies on external services or logic and should be fully
 * serializable.
 */
@NullMarked
public class AutocompleteInput implements UserData {
    @IntDef(
            value = {
                RefineActionUsage.NOT_USED,
                RefineActionUsage.SEARCH_WITH_ZERO_PREFIX,
                RefineActionUsage.SEARCH_WITH_PREFIX,
                RefineActionUsage.SEARCH_WITH_BOTH,
                RefineActionUsage.COUNT
            },
            flag = true)
    @Retention(RetentionPolicy.SOURCE)
    public @interface RefineActionUsage {
        int NOT_USED = 0; // User did not interact with Refine button.
        int SEARCH_WITH_ZERO_PREFIX = 1; // User interacted with Refine button in zero-prefix mode.
        int SEARCH_WITH_PREFIX = 2; // User interacted with Refine button in non-zero-prefix mode.
        int SEARCH_WITH_BOTH = 3; // User interacted with Refine button in both contexts.
        int COUNT = 4;
    }

    private long mUrlFocusTime;
    private GURL mPageUrl;
    private int mPageClassification;
    private String mPageTitle;
    private String mUserText;
    private boolean mAllowExactKeywordMatch;
    private boolean mHasAttachments;
    private boolean mSuppressAutomaticSuggestionsUntilUserStartsTyping;
    private Range<Integer> mSelection;
    private @RefineActionUsage int mRefineActionUsage;
    private boolean mSuggestionsListScrolled;
    private @OmniboxFocusReason int mFocusReason;
    private final SettableNonNullObservableSupplier<@AutocompleteRequestType Integer>
            mRequestTypeSupplier =
                    ObservableSuppliers.createNonNull(AutocompleteRequestType.SEARCH);
    private final SettableNullableObservableSupplier<String> mCurrentKeyword =
            ObservableSuppliers.createNullable();

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
                    PageClassification.NTP_OMNIBOX_COMPOSEBOX_VALUE;
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

    /**
     * Returns the page classification not adjusted for the tools or models.
     *
     * @return The raw page classification.
     */
    public int getRawPageClassification() {
        return mPageClassification;
    }

    /** Returns the current page classification. */
    public int getPageClassification() {
        return switch (mRequestTypeSupplier.get()) {
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

    /** Sets the specific reason that activated the input session. */
    public AutocompleteInput setFocusReason(@OmniboxFocusReason int focusReason) {
        mFocusReason = focusReason;
        return this;
    }

    /** Returns how the input session was activated. */
    public @OmniboxFocusReason int getFocusReason() {
        return mFocusReason;
    }

    /** Set the AutocompleteRequestType */
    public AutocompleteInput setRequestType(@AutocompleteRequestType int type) {
        mRequestTypeSupplier.set(type);
        return this;
    }

    /** Returns the AutocompleteRequestType value. */
    public @AutocompleteRequestType int getRequestType() {
        return mRequestTypeSupplier.get();
    }

    /**
     * Returns the supplier for the AutocompleteRequestType.
     *
     * <p>Use sparingly - to install/remove observers. Readers should use {@see getRequestType()}.
     * Writers should use {@see setRequestType()}.
     */
    public NonNullObservableSupplier<@AutocompleteRequestType Integer> getRequestTypeSupplier() {
        return mRequestTypeSupplier;
    }

    /** Set the current keyword */
    public AutocompleteInput setKeyword(@Nullable String keyword) {
        mCurrentKeyword.set(keyword);
        return this;
    }

    /** Returns the current keyword. */
    public @Nullable String getKeyword() {
        return mCurrentKeyword.get();
    }

    /**
     * Returns the supplier for the current keyword.
     *
     * <p>Use sparingly - to install/remove observers. Readers should use {@see getKeyword()}.
     * Writers should use {@see setKeyword()}.
     */
    public NullableObservableSupplier<String> getKeywordSupplier() {
        return mCurrentKeyword;
    }

    /**
     * Whether the given mode allows "conventional" fulfillment of a valid typed url, i.e.
     * navigating to that url directly. As an example of where this might return false: if if the
     * user types www.foo.com and presses enter with this mode active, they will be taken to some
     * DSE-specific landing page where www.foo.com is the input, not directly to foo.com.
     *
     * @return Whether the request is of a conventional type.
     */
    public boolean isConventionalRequestType() {
        return mRequestTypeSupplier.get() == AutocompleteRequestType.SEARCH;
    }

    /** Returns the Autocomplete Tool to use to fulfill the Request. */
    public /* ToolMode */ int getToolMode() {
        return switch (mRequestTypeSupplier.get()) {
            case AutocompleteRequestType.IMAGE_GENERATION ->
                    mHasAttachments
                            ? ToolMode.TOOL_MODE_IMAGE_GEN_UPLOAD_VALUE
                            : ToolMode.TOOL_MODE_IMAGE_GEN_VALUE;
            default -> ToolMode.TOOL_MODE_UNSPECIFIED_VALUE;
        };
    }

    /**
     * Set the text as currently typed by the User.
     *
     * <p>Allows passing null text to indicate no/empty input. When the new text differs from the
     * existing content of the UserText the selection markers and keyword matching flags are reset.
     * When new text matches the existing text no action is taken.
     *
     * @param text The user-typed text. Null text is automatically replaced with empty string.
     * @return The AutocompleteInput object.
     */
    public AutocompleteInput setUserText(@Nullable String text) {
        if (text == null) text = "";
        if (TextUtils.equals(text, mUserText)) return this;

        boolean oldTextUsesKeywordActivator =
                !TextUtils.isEmpty(mUserText) && TextUtils.indexOf(mUserText, ' ') > 0;
        boolean newTextUsesKeywordActivator =
                !TextUtils.isEmpty(text) && TextUtils.indexOf(text, ' ') > 0;

        // Allow engaging Keyword mode only if the user input introduces first space.
        mAllowExactKeywordMatch |= !oldTextUsesKeywordActivator && newTextUsesKeywordActivator;
        // Suppress Keyword mode when reverting back to the url.
        mAllowExactKeywordMatch &= !(oldTextUsesKeywordActivator && !newTextUsesKeywordActivator);

        mUserText = text;
        // Place cursor at the end of text.
        mSelection = Range.create(text.length(), text.length());
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

    public AutocompleteInput setSelection(int rangeStart, int rangeEnd) {
        mSelection = Range.create(rangeStart, rangeEnd);
        return this;
    }

    public Range<Integer> getSelection() {
        return mSelection;
    }

    /** Returns the current RefineActionUsage. */
    public @RefineActionUsage int getRefineActionUsage() {
        return mRefineActionUsage;
    }

    /**
     * Sets the refine action usage.
     *
     * @param refineActionUsage The new refine action usage.
     */
    public void setRefineActionUsage(@RefineActionUsage int refineActionUsage) {
        mRefineActionUsage = refineActionUsage;
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
        // Selection after all text
        mSelection = Range.create(Integer.MAX_VALUE, Integer.MAX_VALUE);
        mRefineActionUsage = RefineActionUsage.NOT_USED;
        mPageClassification = PageClassification.BLANK_VALUE;
        mFocusReason = OmniboxFocusReason.OMNIBOX_TAP;
        mRequestTypeSupplier.set(AutocompleteRequestType.SEARCH);
        mCurrentKeyword.set(null);
        mUrlFocusTime = 0;
        mSuggestionsListScrolled = false;
        mSuppressAutomaticSuggestionsUntilUserStartsTyping = false;

        return this;
    }

    public long getUrlFocusTime() {
        return mUrlFocusTime;
    }

    public AutocompleteInput setUrlFocusTime(long urlFocusTime) {
        mUrlFocusTime = urlFocusTime;
        return this;
    }

    public boolean isSuggestionsListScrolled() {
        return mSuggestionsListScrolled;
    }

    public void setSuggestionsListScrolled() {
        mSuggestionsListScrolled = true;
    }

    /**
     * Whether to instantly show suggestions for the supplied input (false), or wait until the user
     * actually began typing query (true).
     */
    public boolean shouldSuppressAutomaticSuggestionsUntilUserStartsTyping() {
        return mSuppressAutomaticSuggestionsUntilUserStartsTyping;
    }

    public AutocompleteInput setSuppressAutomaticSuggestionsUntilUserStartsTyping(
            boolean suppress) {
        mSuppressAutomaticSuggestionsUntilUserStartsTyping = suppress;
        return this;
    }
}
