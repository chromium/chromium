// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.omnibox;

import android.text.TextUtils;

import androidx.annotation.IntDef;
import androidx.annotation.VisibleForTesting;

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
import org.chromium.components.search_engines.StarterPackId;
import org.chromium.url.GURL;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.Locale;
import java.util.Objects;

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

    @IntDef({
        AutocompleteState.DISABLED,
        AutocompleteState.STANDBY,
        AutocompleteState.ENABLED,
        AutocompleteState.STANDBY_NO_FOCUS
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface AutocompleteState {
        /** Fully disabled autocompletion. */
        int DISABLED = 0;

        /** Autocompletion disabled until user starts typing. */
        int STANDBY = 1;

        /** Fully enabled autocompletion, including zero-state suggestions. */
        int ENABLED = 2;

        /** Autocompletion disabled until user starts typing, and does not focus edittext. */
        int STANDBY_NO_FOCUS = 3;
    }

    public static class SiteSearchData {
        public final String keyword;
        public final String fullName;
        public final boolean enteredViaSpace;
        public final @StarterPackId int starterPackId;

        public SiteSearchData(String keyword, String fullName) {
            this(keyword, fullName, false, StarterPackId.NONE);
        }

        public SiteSearchData(String keyword, String fullName, boolean enteredViaSpace) {
            this(keyword, fullName, enteredViaSpace, StarterPackId.NONE);
        }

        public SiteSearchData(
                String keyword,
                String fullName,
                boolean enteredViaSpace,
                @StarterPackId int starterPackId) {
            this.keyword = keyword;
            this.fullName = fullName;
            this.enteredViaSpace = enteredViaSpace;
            this.starterPackId = starterPackId;
        }

        @Override
        public boolean equals(@Nullable Object o) {
            if (this == o) return true;
            if (!(o instanceof SiteSearchData that)) return false;
            return enteredViaSpace == that.enteredViaSpace
                    && Objects.equals(keyword, that.keyword)
                    && Objects.equals(fullName, that.fullName);
        }

        @Override
        public int hashCode() {
            return Objects.hash(keyword, fullName, enteredViaSpace);
        }
    }

    // LINT.IfChange(Members)
    private long mUrlFocusTime;
    private GURL mPageUrl;
    private int mPageClassification;
    private String mPageTitle;
    private boolean mAllowExactKeywordMatch;
    private boolean mHasAttachments;
    private @AutocompleteState int mAutocompleteState = AutocompleteState.ENABLED;
    private TextSelection mSelection;
    private @RefineActionUsage int mRefineActionUsage;
    private boolean mSuggestionsListScrolled;
    private @OmniboxFocusReason int mFocusReason;
    private /* ModelMode */ int mModelMode;

    private String mInitialUserText = "";
    private final SettableNonNullObservableSupplier<String> mUserText =
            ObservableSuppliers.createNonNull("");
    private @Nullable String mPreviewText;
    private final SettableNonNullObservableSupplier<Boolean> mAllowUserTextAutocompletion =
            ObservableSuppliers.createNonNull(true);
    private final SettableNonNullObservableSupplier<@AutocompleteRequestType Integer>
            mRequestTypeSupplier =
                    ObservableSuppliers.createNonNull(AutocompleteRequestType.SEARCH);
    private final SettableNullableObservableSupplier<SiteSearchData> mSiteSearchData =
            ObservableSuppliers.createNullable();

    // LINT.ThenChange(:CopyFrom)

    @VisibleForTesting
    public AutocompleteInput() {
        reset();
    }

    public AutocompleteInput(@OmniboxFocusReason int focusReason) {
        reset();
        mFocusReason = focusReason;
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

    /**
     * Mutates this object to have the same values as {@code other}. Observers of the suppliers will
     * not be copied, only the current values.
     *
     * @param other The {@link AutocompleteInput} to copy values from.
     */
    // LINT.IfChange(CopyFrom)
    public void copyFrom(AutocompleteInput other) {
        mUrlFocusTime = other.mUrlFocusTime;
        mPageUrl = other.mPageUrl;
        mPageClassification = other.mPageClassification;
        mPageTitle = other.mPageTitle;
        mAllowExactKeywordMatch = other.mAllowExactKeywordMatch;
        mHasAttachments = other.mHasAttachments;
        mAutocompleteState = other.mAutocompleteState;
        mSelection = other.mSelection; // Copied.
        mRefineActionUsage = other.mRefineActionUsage;
        mSuggestionsListScrolled = other.mSuggestionsListScrolled;
        mFocusReason = other.mFocusReason;
        mModelMode = other.mModelMode;
        mUserText.set(other.mUserText.get());
        mPreviewText = other.mPreviewText;
        mAllowUserTextAutocompletion.set(other.mAllowUserTextAutocompletion.get());
        mInitialUserText = other.mInitialUserText;
        mRequestTypeSupplier.set(other.mRequestTypeSupplier.get());
        mSiteSearchData.set(other.mSiteSearchData.get());
    }

    // LINT.ThenChange(:Members)

    private int getComposeboxEquivalentOfPageClassification() {
        return switch (mPageClassification) {
            // LINT.IfChange(FuseboxSupportedPageClassifications)
            case PageClassification.INSTANT_NTP_WITH_OMNIBOX_AS_STARTING_FOCUS_VALUE ->
                    PageClassification.NTP_OMNIBOX_COMPOSEBOX_VALUE;
            case PageClassification.SEARCH_RESULT_PAGE_NO_SEARCH_TERM_REPLACEMENT_VALUE ->
                    PageClassification.SRP_OMNIBOX_COMPOSEBOX_VALUE;
            case PageClassification.CO_BROWSING_COMPOSEBOX_VALUE ->
                    PageClassification.CO_BROWSING_COMPOSEBOX_VALUE;
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
        return ToolModeUtils.isAimRequest(mRequestTypeSupplier.get())
                ? getComposeboxEquivalentOfPageClassification()
                : mPageClassification;
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
    public AutocompleteInput setSiteSearchData(@Nullable SiteSearchData siteSearchData) {
        if (Objects.equals(siteSearchData, mSiteSearchData.get())) return this;
        mSiteSearchData.set(siteSearchData);
        return this;
    }

    /** Returns the current SiteSearchData. */
    public @Nullable SiteSearchData getSiteSearchData() {
        return mSiteSearchData.get();
    }

    /**
     * Returns the supplier for the current keyword.
     *
     * <p>Use sparingly - to install/remove observers. Readers should use {@see getKeyword()}.
     * Writers should use {@see setSiteSearchData()}.
     */
    public NullableObservableSupplier<SiteSearchData> getSiteSearchDataSupplier() {
        return mSiteSearchData;
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

    /** Returns the Autocomplete Tool that is currently selected. */
    public int getToolMode() {
        return ToolModeUtils.getToolModeForRequestType(getRequestType(), mHasAttachments);
    }

    /**
     * Set the text as currently typed by the User.
     *
     * <p>Allows passing null text to indicate no/empty input. When the new text differs from the
     * existing content of the UserText the selection markers and keyword matching flags are reset.
     * When new text matches the existing text no action is taken.
     *
     * @param text The user-typed text. Null text is automatically replaced with empty string. Note
     *     that if the site search is triggered, the text will only contains the content after the
     *     keyword and space.
     * @return The AutocompleteInput object.
     */
    public AutocompleteInput setUserText(@Nullable String text) {
        if (text == null) text = "";

        mPreviewText = null;

        String oldText = mUserText.get();
        if (TextUtils.equals(text, oldText)) return this;

        boolean oldTextUsesKeywordActivator = allowExactKeywordTrigger(oldText);
        boolean newTextUsesKeywordActivator = allowExactKeywordTrigger(text);

        // Allow engaging Keyword mode only if the user input introduces first space.
        mAllowExactKeywordMatch |= !oldTextUsesKeywordActivator && newTextUsesKeywordActivator;
        // Suppress Keyword mode when reverting back to the url.
        mAllowExactKeywordMatch &= !(oldTextUsesKeywordActivator && !newTextUsesKeywordActivator);

        mUserText.set(text);
        // Place cursor at the end of text.
        mSelection = TextSelection.SELECT_END;
        return this;
    }

    /**
     * Set the Initial Input - the default value to fall back to if the input is reset.
     *
     * <p>This is the default "revert-to" value.
     */
    public AutocompleteInput setInitialUserText(String userText) {
        mInitialUserText = userText;
        return this;
    }

    /** Returns the Initial Input - the default value to fall back to if the input is reset. */
    public String getInitialUserText() {
        return mInitialUserText;
    }

    /** Returns whether exact keyword match is allowed with current input. */
    public boolean allowExactKeywordMatch() {
        return mAllowExactKeywordMatch || getSiteSearchData() != null;
    }

    /**
     * Returns the user text formatted for autocomplete.
     *
     * <p>When the user is in Keyword mode (e.g., Site Search), this method concatenates the keyword
     * and user text. This concatenation approach mirrors how Desktop/Views handles it: the UI
     * separates the keyword into a chip visually, but silently prepends it to the query string
     * right before passing it to the C++ controller. Doing it this way keeps the JNI boundary and
     * cross-platform parsing logic unchanged.
     *
     * @return The text to be sent to the AutocompleteController.
     */
    public String getTextForAutocomplete() {
        SiteSearchData siteSearchData = getSiteSearchData();
        if (siteSearchData != null) {
            return siteSearchData.keyword + " " + mUserText.get();
        }
        return mUserText.get();
    }

    /**
     * Calculates the adjusted cursor position for autocomplete.
     *
     * <p>Adjusts the cursor position to account for the prepended keyword.
     *
     * @param currentCursorPosition The cursor position in the UI text field.
     * @return The adjusted cursor position.
     */
    public int getCursorPositionForAutocomplete(int currentCursorPosition) {
        SiteSearchData siteSearchData = getSiteSearchData();
        if (siteSearchData != null && currentCursorPosition >= 0) {
            // It's possible the UI text has not synchronously updated yet, meaning the reported
            // cursor position is out of bounds for the logical text. Cap it to the length of the
            // user text.
            int safeCursorPosition = Math.min(currentCursorPosition, mUserText.get().length());
            return safeCursorPosition + siteSearchData.keyword.length() + 1;
        }
        return currentCursorPosition;
    }

    /** Returns the text as currently typed by the User. */
    public String getUserText() {
        return mUserText.get();
    }

    /** Returns the supplier for the text as currently typed by the User. */
    public NonNullObservableSupplier<String> getUserTextSupplier() {
        return mUserText;
    }

    /**
     * Sets the preview text. If the preview text is empty or same as user text, it is reset to
     * null.
     *
     * @param text The preview text.
     * @return The AutocompleteInput object.
     */
    public AutocompleteInput setPreviewText(@Nullable String text) {
        if (text == null || TextUtils.equals(mUserText.get(), text)) {
            mPreviewText = null;
        } else {
            mPreviewText = text;
        }
        return this;
    }

    /** Returns the preview text if set, otherwise the user text. */
    public String getPreviewText() {
        return mPreviewText == null ? mUserText.get() : mPreviewText;
    }

    /** Returns whether there is an active preview text. */
    public boolean hasPreviewText() {
        return mPreviewText != null;
    }

    /** Resets the preview text. */
    public AutocompleteInput resetPreviewText() {
        return setPreviewText(null);
    }

    /** Commits the preview text as the user text. */
    public AutocompleteInput commitPreviewText() {
        if (hasPreviewText()) {
            String textToCommit = mPreviewText;
            setUserText(textToCommit);
        }
        return this;
    }

    /** Sets whether user text should be autocompleted. */
    public AutocompleteInput setAllowUserTextAutocompletion(boolean shouldAllow) {
        mAllowUserTextAutocompletion.set(shouldAllow);
        return this;
    }

    /** Returns whether user text can be autocompleted. */
    public boolean shouldAllowUserTextAutocompletion() {
        return mAllowUserTextAutocompletion.get();
    }

    /** Returns the supplier for the autocompletion status. */
    public NonNullObservableSupplier<Boolean> getShouldAllowUserTextAutocompletionSupplier() {
        return mAllowUserTextAutocompletion;
    }

    /** Returns whether the current context includes user-typed text. */
    public boolean isInZeroPrefixContext() {
        return getSiteSearchData() == null
                && (TextUtils.isEmpty(mUserText.get())
                        || TextUtils.equals(mUserText.get(), mInitialUserText));
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

    public AutocompleteInput setSelection(TextSelection selection) {
        mSelection = selection;
        return this;
    }

    public TextSelection getSelection() {
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
        mAllowExactKeywordMatch = false;
        mPageUrl = GURL.emptyGURL();
        mPageTitle = "";
        mHasAttachments = false;
        // Selection after all text
        mSelection = TextSelection.SELECT_END;
        mRefineActionUsage = RefineActionUsage.NOT_USED;
        mPageClassification = PageClassification.BLANK_VALUE;
        mFocusReason = OmniboxFocusReason.OMNIBOX_TAP;
        mUserText.set("");
        mPreviewText = null;
        mAllowUserTextAutocompletion.set(true);
        mRequestTypeSupplier.set(AutocompleteRequestType.SEARCH);
        mSiteSearchData.set(null);
        mUrlFocusTime = 0;
        mSuggestionsListScrolled = false;
        mAutocompleteState = AutocompleteState.ENABLED;

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
     * Returns the current {@link AutocompleteState}. Internally tracks and updates own state to
     * reflect typing started.
     */
    public @AutocompleteState int getAutocompleteState() {
        if ((mAutocompleteState == AutocompleteState.STANDBY
                        || mAutocompleteState == AutocompleteState.STANDBY_NO_FOCUS)
                && !TextUtils.equals(mUserText.get(), mInitialUserText)) {
            mAutocompleteState = AutocompleteState.ENABLED;
        }
        return mAutocompleteState;
    }

    /** Sets the {@link AutocompleteState}. */
    public AutocompleteInput setAutocompleteState(@AutocompleteState int state) {
        mAutocompleteState = state;
        return this;
    }

    /** Returns the current model mode or MODEL_MODE_UNSPECIFIED if never set. */
    public /* ModelMode */ int getModelMode() {
        return mModelMode;
    }

    /** Sets the ModelMode that should be used. */
    public void setModelMode(int modelMode) {
        mModelMode = modelMode;
    }

    @VisibleForTesting
    static boolean allowExactKeywordTrigger(String text) {
        if (TextUtils.isEmpty(text)) return false;
        // Given test should at least contains keyword + space to allow keyword match.
        if (text.length() <= 1 || !text.endsWith(" ")) return false;

        // Checks if there is exactly one space in the input.  If a user triggers site search
        // (e.g. "yahoo "), then deletes the chip via <backspace> and continues typing a multi-word
        // query ("yahoo some query"), we must prevent the first word from falsely re-triggering the
        // keyword match.
        return text.indexOf(' ') == text.lastIndexOf(' ');
    }
}
