// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox;

import android.content.Context;
import android.text.TextUtils;

import androidx.annotation.ColorInt;
import androidx.annotation.VisibleForTesting;
import androidx.fragment.app.Fragment;

import org.chromium.base.Callback;
import org.chromium.build.annotations.EnsuresNonNullIf;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.omnibox.UrlBar.ScrollType;
import org.chromium.chrome.browser.omnibox.UrlBar.UrlBarDelegate;
import org.chromium.chrome.browser.omnibox.UrlBar.UrlBarTextContextMenuDelegate;
import org.chromium.chrome.browser.omnibox.UrlBarProperties.AutocompleteText;
import org.chromium.chrome.browser.omnibox.UrlBarProperties.UrlBarTextState;
import org.chromium.chrome.browser.omnibox.styles.OmniboxResourceProvider;
import org.chromium.chrome.browser.search_engines.settings.SearchEngineSettings;
import org.chromium.chrome.browser.search_engines.settings.SiteSearchSettings;
import org.chromium.chrome.browser.settings.SettingsNavigationFactory;
import org.chromium.chrome.browser.ui.theme.BrandedColorScheme;
import org.chromium.components.browser_ui.styles.SemanticColorUtils;
import org.chromium.components.omnibox.AutocompleteInput;
import org.chromium.components.omnibox.AutocompleteInput.DisplayState;
import org.chromium.components.omnibox.OmniboxFeatures;
import org.chromium.components.omnibox.OmniboxUrlEmphasizer.UrlEmphasisSpan;
import org.chromium.components.omnibox.TextSelection;
import org.chromium.ui.base.Clipboard;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.url.GURL;

import java.util.Objects;

/** Handles collecting and pushing state information to the UrlBar model. */
@NullMarked
class UrlBarMediator implements UrlBarTextContextMenuDelegate {
    private final Context mContext;
    private final PropertyModel mModel;

    private @Nullable AutocompleteInput mCurrentInput;
    private UrlBarData mUrlBarData = UrlBarData.EMPTY;
    private @ScrollType int mScrollType = ScrollType.NO_SCROLL;
    private TextSelection mSelection = TextSelection.SELECT_ALL;

    // For NTP, when in un-focus state, the search text hint color is fixed for the real search box
    // and we couldn't change it by the branded color scheme.
    private boolean mIsHintTextFixedForNtp;
    private boolean mShowOriginOnly;
    private boolean mIsReparenting;
    private final @Nullable Callback<String> mTextChangeListener;
    private final @Nullable Callback<UrlBarTextChangeInfo> mRichTextChangeListener;
    private final Callback<@DisplayState Integer> mDisplayStateObserver =
            this::onDisplayStateChanged;

    /**
     * Creates a URLBarMediator.
     *
     * @param context The current Android's context.
     * @param model MVC property model to write changes to.
     * @param textChangeListener The listener for text changes.
     * @param richTextChangeListener The listener for rich text changes.
     */
    public UrlBarMediator(
            Context context,
            PropertyModel model,
            @Nullable Callback<String> textChangeListener,
            @Nullable Callback<UrlBarTextChangeInfo> richTextChangeListener) {
        mContext = context;
        mModel = model;
        mTextChangeListener = textChangeListener;
        mRichTextChangeListener = richTextChangeListener;

        mModel.set(UrlBarProperties.TEXT_CONTEXT_MENU_DELEGATE, this);
        mModel.set(UrlBarProperties.ALLOW_MULTILINE_INPUT, false);
        mModel.set(UrlBarProperties.HAS_URL_SUGGESTIONS, false);
        mModel.set(UrlBarProperties.TEXT_CHANGE_LISTENER, this::onTextChanged);
        mModel.set(UrlBarProperties.RICH_TEXT_CHANGE_LISTENER, this::onRichTextChanged);
        mModel.set(UrlBarProperties.SHOW_HINT_TEXT, true);
        if (DeviceFormFactor.isNonMultiDisplayContextOnTablet(context)) {
            mModel.set(
                    UrlBarProperties.MANAGE_SEARCH_ENGINES_CALLBACK,
                    this::onManageSearchEnginesClicked);
        }
        setBrandedColorScheme(BrandedColorScheme.APP_DEFAULT);
        pushTextToModel(/* originChanged= */ false);
    }

    public void destroy() {
        if (mCurrentInput != null) {
            mCurrentInput.getDisplayStateSupplier().removeObserver(mDisplayStateObserver);
        }
        mModel.set(UrlBarProperties.TEXT_CONTEXT_MENU_DELEGATE, null);
        mModel.set(UrlBarProperties.TEXT_CHANGE_LISTENER, null);
        mModel.set(UrlBarProperties.RICH_TEXT_CHANGE_LISTENER, null);
        mModel.set(UrlBarProperties.MANAGE_SEARCH_ENGINES_CALLBACK, null);
    }

    /** Signals that the Omnibox input session has begun. */
    void beginInput(FuseboxSessionState sessionState) {
        if (mCurrentInput != null) {
            mCurrentInput.getDisplayStateSupplier().removeObserver(mDisplayStateObserver);
        }
        mCurrentInput = sessionState.getAutocompleteInput();
        mCurrentInput
                .getDisplayStateSupplier()
                .addSyncObserverAndCallIfNonNull(mDisplayStateObserver);
        pushCurrentInputToModel();
    }

    /** Signals that the Omnibox input session has ended. */
    void endInput() {
        if (!isInInputSession()) return;
        mCurrentInput.getDisplayStateSupplier().removeObserver(mDisplayStateObserver);
        mModel.set(UrlBarProperties.ALLOW_MULTILINE_INPUT, false);
        var pageUrl = mCurrentInput.getPageUrl();
        mCurrentInput = null;
        var data = UrlBarData.forUrl(pageUrl);
        setUrlBarData(data, ScrollType.SCROLL_TO_TLD, TextSelection.SELECT_END);
    }

    private void onDisplayStateChanged(@DisplayState int displayState) {
        boolean allowMultiline = displayState == DisplayState.SUGGESTIONS;
        mModel.set(UrlBarProperties.ALLOW_MULTILINE_INPUT, allowMultiline);
    }

    /** Sets the current selection for the active input session. */
    void setSelection(TextSelection selection) {
        if (mCurrentInput != null) {
            mCurrentInput.setSelection(selection);
        }
        mSelection = selection;
    }

    /**
     * Signals that the UrlBar is being relocated to a new parent.
     *
     * @param currentSelection The current text selection of the UrlBar prior to reparenting.
     */
    void startReparenting(TextSelection currentSelection) {
        mIsReparenting = true;
        setSelection(currentSelection);
    }

    /**
     * Signals that the UrlBar has finished being relocated to a new parent.
     *
     * @param postReparentingFocus Whether the UrlBar should be focused.
     */
    void finishReparenting(boolean postReparentingFocus) {
        mIsReparenting = false;
        if (postReparentingFocus && !isInInputSession()) {
            pushTextToModel(/* originChanged= */ false);
        }
    }

    /** Returns whether the UrlBar is currently being reparented. */
    boolean isReparenting() {
        return mIsReparenting;
    }

    /* package */ void pushCurrentInputToModel() {
        if (!isInInputSession()) return;
        UrlBarDelegate delegate = mModel.get(UrlBarProperties.DELEGATE);
        assert delegate != null;
        UrlBarData data = delegate.getUrlBarDataForCurrentInput();
        setUrlBarData(data, ScrollType.SCROLL_TO_BEGINNING, mCurrentInput.getSelection());
        if (mCurrentInput.hasPreviewText()) {
            String userText = mCurrentInput.getUserText();
            String previewText = mCurrentInput.getPreviewText();
            if (previewText.startsWith(userText)) {
                String inlineAutocomplete = previewText.substring(userText.length());
                setAutocompleteText(
                        userText,
                        inlineAutocomplete,
                        /* additionalText= */ null,
                        /* siteSearchLabel= */ null);
            }
        }
    }

    @EnsuresNonNullIf("mCurrentInput")
    /* package */ boolean isInInputSession() {
        return mCurrentInput != null;
    }

    private void onTextChanged(String text) {
        // Keep mUrlBarData synchronized with user-typed text during an active input session.
        // This ensures mUrlBarData accurately reflects the current editor content so that
        // setUrlBarData() can safely deduplicate redundant updates without incorrectly dropping
        // legitimate changes (e.g. when tapping the Delete button to clear typed text).
        if (isInInputSession()) {
            UrlBarData typedData = UrlBarData.forNonUrlText(text);
            if (!isNewTextEquivalentToExistingText(mUrlBarData, typedData)) {
                mUrlBarData = typedData;
            }
        }
        if (mTextChangeListener != null) {
            mTextChangeListener.onResult(text);
        }
        if (isInInputSession()) {
            mSelection = mCurrentInput.getSelection();
        }
        updateShowHintText(text);
    }

    private void onRichTextChanged(UrlBarTextChangeInfo info) {
        if (mRichTextChangeListener != null) {
            mRichTextChangeListener.onResult(info);
        }
        updateShowHintText(info.getText());
    }

    private void updateShowHintText(String text) {
        boolean showHintText = !isInInputSession() || text.isEmpty();
        mModel.set(UrlBarProperties.SHOW_HINT_TEXT, showHintText);
    }

    private void onManageSearchEnginesClicked() {
        Class<? extends Fragment> fragment =
                OmniboxFeatures.sOmniboxSiteSearch.isEnabled()
                        ? SiteSearchSettings.class
                        : SearchEngineSettings.class;
        SettingsNavigationFactory.createSettingsNavigation().startSettings(mContext, fragment);
    }

    /**
     * Updates the text content of the UrlBar.
     *
     * @param data The new data to be displayed.
     * @param scrollType The scroll type that should be applied to the data.
     * @param selection Specifies the range of text to be selected when focused.
     * @return Whether this data differs from the previously passed in values.
     */
    public boolean setUrlBarData(
            UrlBarData data, @ScrollType int scrollType, TextSelection selection) {
        assert data != null;

        if (data.originEndIndex == data.originStartIndex) {
            scrollType = ScrollType.SCROLL_TO_BEGINNING;
        }

        // Do not scroll to the end of the host for URLs such as data:, javascript:, etc...
        if (data.url != null
                && data.displayText != null
                && data.originEndIndex == data.displayText.length()) {
            String scheme = data.url.getScheme();
            if (!TextUtils.isEmpty(scheme) && !UrlBarData.SCHEMES_TO_SPLIT.contains(scheme)) {
                scrollType = ScrollType.SCROLL_TO_BEGINNING;
            }
        }

        boolean textEquivalent = isNewTextEquivalentToExistingText(mUrlBarData, data);
        boolean scrollTypeEquivalent =
                (mScrollType == scrollType)
                        || (TextUtils.isEmpty(data.displayText)
                                && TextUtils.isEmpty(mUrlBarData.displayText));
        boolean selectionEquivalent = !isInInputSession() || mSelection.equals(selection);

        if (textEquivalent && scrollTypeEquivalent && selectionEquivalent) {
            return false;
        }

        boolean originChanged = !Objects.equals(getOrigin(mUrlBarData.url), getOrigin(data.url));
        mUrlBarData = data;
        mScrollType = scrollType;
        mSelection = selection;

        pushTextToModel(originChanged);
        return true;
    }

    UrlBarData getUrlBarData() {
        return mUrlBarData;
    }

    /* package */ void pushTextToModel(boolean originChanged) {
        CharSequence text;
        if (mShowOriginOnly && mUrlBarData.originStartIndex != mUrlBarData.originEndIndex) {
            text =
                    mUrlBarData.displayText.subSequence(
                            mUrlBarData.originStartIndex, mUrlBarData.originEndIndex);
        } else {
            text =
                    !isInInputSession()
                            ? mUrlBarData.displayText
                            : mUrlBarData.getEditingOrDisplayText();
        }
        CharSequence textForAutofillServices = text;

        if (!(isInInputSession() || TextUtils.isEmpty(text) || mUrlBarData.url == null)) {
            textForAutofillServices = mUrlBarData.url.getSpec();
        }

        @ScrollType int scrollType = isInInputSession() ? ScrollType.NO_SCROLL : mScrollType;
        if (text == null) text = "";

        UrlBarTextState state =
                new UrlBarTextState(
                        text,
                        textForAutofillServices,
                        scrollType,
                        mUrlBarData.originEndIndex,
                        mSelection,
                        originChanged);
        mModel.set(UrlBarProperties.TEXT_STATE, state);
        updateShowHintText(text.toString());
    }

    @VisibleForTesting
    protected static boolean isNewTextEquivalentToExistingText(
            UrlBarData existingUrlData, UrlBarData newUrlData) {
        if (existingUrlData == null) return newUrlData == null;
        if (newUrlData == null) return false;

        if (!TextUtils.equals(existingUrlData.editingText, newUrlData.editingText)) return false;

        CharSequence existingCharSequence = existingUrlData.displayText;
        CharSequence newCharSequence = newUrlData.displayText;
        if (existingCharSequence == null) return newCharSequence == null;

        // Regardless of focus state, ensure the text content is the same.
        if (!TextUtils.equals(existingCharSequence, newCharSequence)) return false;

        // If both existing and new text is empty, then treat them equal regardless of their
        // spanned state.
        if (TextUtils.isEmpty(newCharSequence)) return true;

        // When not focused, compare the emphasis spans applied to the text to determine
        // equality. Internally, TextView applies many additional spans that need to be
        // ignored for this comparison to be useful, so this is scoped to only the span types
        // applied by our UI.
        return OmniboxViewUtil.haveEquivalentSpans(
                existingCharSequence, newCharSequence, UrlEmphasisSpan.class);
    }

    /**
     * Sets the autocomplete text to be shown.
     *
     * @param userText The existing user text.
     * @param autocompleteText The text to be appended to the user text.
     * @param additionalText This string is displayed adjacent to the omnibox if this match is the
     *     default. Will usually be URL when autocompleting a title, and empty otherwise.
     * @param siteSearchLabel Text label displayed for site search in the URL bar.
     */
    public void setAutocompleteText(
            String userText,
            @Nullable String autocompleteText,
            @Nullable String additionalText,
            @Nullable String siteSearchLabel) {
        if (!isInInputSession()) {
            assert false : "Should not update autocomplete text when not focused";
            return;
        }
        mModel.set(
                UrlBarProperties.AUTOCOMPLETE_TEXT,
                new AutocompleteText(userText, autocompleteText, additionalText, siteSearchLabel));
    }

    private @Nullable GURL getOrigin(@Nullable GURL gurl) {
        return gurl != null ? gurl.getOrigin() : null;
    }

    /**
     * Sets the color scheme.
     *
     * @param brandedColorScheme The {@link @BrandedColorScheme}.
     */
    public void setBrandedColorScheme(@BrandedColorScheme int brandedColorScheme) {
        final @ColorInt int textColor =
                OmniboxResourceProvider.getUrlBarPrimaryTextColor(mContext, brandedColorScheme);
        mModel.set(UrlBarProperties.TEXT_COLOR, textColor);

        if (!mIsHintTextFixedForNtp) {
            @ColorInt
            int hintTextColor =
                    OmniboxResourceProvider.getUrlBarHintTextColor(mContext, brandedColorScheme);
            mModel.set(UrlBarProperties.HINT_TEXT_COLOR, hintTextColor);
        }
    }

    /**
     * Sets whether to use incognito colors.
     *
     * @param incognitoColorsEnabled Whether to use incognito colors.
     */
    public void setIncognitoColorsEnabled(boolean incognitoColorsEnabled) {
        mModel.set(UrlBarProperties.INCOGNITO_COLORS_ENABLED, incognitoColorsEnabled);
    }

    /** Sets whether the view allows user focus. */
    public void setAllowFocus(boolean allowFocus) {
        mModel.set(UrlBarProperties.ALLOW_FOCUS, allowFocus);
    }

    /** Set the listener to be notified for URL direction changes. */
    public void setUrlDirectionListener(Callback<Integer> listener) {
        mModel.set(UrlBarProperties.URL_DIRECTION_LISTENER, listener);
    }

    @Override
    public @Nullable String getReplacementCutCopyText(String currentText, TextSelection selection) {
        if (mUrlBarData.url == null) return null;

        // Replace the cut/copy text only applies if the user selected from the beginning of the
        // display text.
        int minSel = selection.getLower();
        int maxSel = selection.getUpper();

        if (minSel != 0) return null;

        // Trim to just the currently selected text as that is the only text we are replacing.
        currentText = currentText.substring(minSel, maxSel);

        UrlBarDelegate delegate = mModel.get(UrlBarProperties.DELEGATE);
        assert delegate != null;
        String replacement = delegate.getReplacementCutCopyText(currentText, selection);
        if (replacement != null) return replacement;

        String formattedUrlLocation;
        String originalUrlLocation;

        formattedUrlLocation =
                getUrlContentsPrePath(
                        mUrlBarData.getEditingOrDisplayText().toString(),
                        mUrlBarData.url.getHost());
        originalUrlLocation =
                getUrlContentsPrePath(mUrlBarData.url.getSpec(), mUrlBarData.url.getHost());

        // If we are copying/cutting the full previously formatted URL, reset the URL
        // text before initiating the TextViews handling of the context menu.
        //
        // Example:
        //    Original display text: www.example.com
        //    Original URL:          http://www.example.com
        //
        // Editing State:
        //    www.example.com/blah/foo
        //    |<--- Selection --->|
        //
        // Resulting clipboard text should be:
        //    http://www.example.com/blah/
        //
        // As long as the full original text was selected, it will replace that with the original
        // URL and keep any further modifications by the user.
        if (!currentText.startsWith(formattedUrlLocation)
                || maxSel < formattedUrlLocation.length()) {
            return null;
        }

        return originalUrlLocation + currentText.substring(formattedUrlLocation.length());
    }

    @Override
    public @Nullable String getTextToPaste() {
        String text = Clipboard.getInstance().getCoercedText();
        return text != null ? sanitizeTextForPaste(text) : null;
    }

    /**
     * @param hasSuggestions Whether suggestions are showing in the URL bar.
     */
    public void onUrlBarSuggestionsChanged(boolean hasSuggestions) {
        mModel.set(UrlBarProperties.HAS_URL_SUGGESTIONS, hasSuggestions);
    }

    @VisibleForTesting
    protected String sanitizeTextForPaste(String text) {
        return OmniboxViewUtil.sanitizeTextForPaste(text);
    }

    /**
     * Returns the portion of the URL that precedes the path/query section of the URL.
     *
     * @param url The url to be used to find the preceding portion.
     * @param host The host to be located in the URL to determine the location of the path.
     * @return The URL contents that precede the path (or the passed in URL if the host is not
     *     found).
     */
    private static String getUrlContentsPrePath(String url, String host) {
        int hostIndex = url.indexOf(host);
        if (hostIndex == -1) return url;

        int pathIndex = url.indexOf('/', hostIndex);
        if (pathIndex <= 0) return url;

        return url.substring(0, pathIndex);
    }

    /**
     * Sets search box hint text color to brandedColorScheme.
     *
     * @param brandedColorScheme The {@link @BrandedColorScheme}.
     */
    void setUrlBarHintTextColorForDefault(@BrandedColorScheme int brandedColorScheme) {
        mIsHintTextFixedForNtp = false;
        setBrandedColorScheme(brandedColorScheme);
    }

    /** Sets search box hint text color to be colorOnSurface for NTP's un-focus state. */
    void setUrlBarHintTextColorForNtp() {
        mIsHintTextFixedForNtp = true;
        final @ColorInt int hintTextColor = SemanticColorUtils.getDefaultTextColor(mContext);
        mModel.set(UrlBarProperties.HINT_TEXT_COLOR, hintTextColor);
    }

    /** Sets the search box hint text. */
    void setUrlBarHintText(CharSequence hintText) {
        mModel.set(UrlBarProperties.HINT_TEXT, hintText);
    }

    void setShowOriginOnly(boolean showOriginOnly) {
        // TODO(https://crbm/411135455): Reconsider the disparate mechanisms we have for UrlBar
        // truncation.
        mShowOriginOnly = showOriginOnly;
        pushTextToModel(/* originChanged= */ false);
    }

    void setUseSmallText(boolean useSmallText) {
        mModel.set(UrlBarProperties.USE_SMALL_TEXT, useSmallText);
    }

    /** Sets the accessibility warning text. */
    public void setAccessibilityWarning(@Nullable String warning) {
        mModel.set(UrlBarProperties.ACCESSIBILITY_WARNING, warning);
    }
}
