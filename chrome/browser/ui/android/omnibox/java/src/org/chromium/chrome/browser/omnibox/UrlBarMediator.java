// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox;

import android.content.ClipData;
import android.content.ClipboardManager;
import android.content.Context;
import android.text.Spanned;
import android.text.TextUtils;
import android.view.View.OnKeyListener;

import androidx.annotation.ColorInt;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.Callback;
import org.chromium.base.ContextUtils;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.omnibox.UrlBar.ScrollType;
import org.chromium.chrome.browser.omnibox.UrlBar.UrlBarDelegate;
import org.chromium.chrome.browser.omnibox.UrlBar.UrlBarTextContextMenuDelegate;
import org.chromium.chrome.browser.omnibox.UrlBarProperties.AutocompleteText;
import org.chromium.chrome.browser.omnibox.UrlBarProperties.UrlBarTextState;
import org.chromium.chrome.browser.omnibox.styles.OmniboxResourceProvider;
import org.chromium.chrome.browser.search_engines.settings.SiteSearchSettings;
import org.chromium.chrome.browser.settings.SettingsNavigationFactory;
import org.chromium.chrome.browser.ui.theme.BrandedColorScheme;
import org.chromium.components.browser_ui.styles.SemanticColorUtils;
import org.chromium.components.omnibox.OmniboxFeatures;
import org.chromium.components.omnibox.OmniboxUrlEmphasizer.UrlEmphasisSpan;
import org.chromium.components.omnibox.TextSelection;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.url.GURL;

import java.util.Objects;

/** Handles collecting and pushing state information to the UrlBar model. */
@NullMarked
class UrlBarMediator implements UrlBarTextContextMenuDelegate {
    private final Context mContext;
    private final PropertyModel mModel;
    private final Callback<Boolean> mOnFocusChangeCallback;

    private boolean mHasFocus;

    private UrlBarData mUrlBarData = UrlBarData.EMPTY;
    private @ScrollType int mScrollType = ScrollType.NO_SCROLL;
    private TextSelection mSelection = TextSelection.SELECT_ALL;

    // For NTP, when in un-focus state, the search text hint color is fixed for the real search box
    // and we couldn't change it by the branded color scheme.
    private boolean mIsHintTextFixedForNtp;
    private boolean mShowOriginOnly;
    private final @Nullable Callback<String> mTextChangeListener;
    private final @Nullable Callback<UrlBarTextChangeInfo> mRichTextChangeListener;
    private boolean mIsReparenting;

    /**
     * Creates a URLBarMediator.
     *
     * @param context The current Android's context.
     * @param model MVC property model to write changes to.
     * @param focusChangeCallback The callback that will be notified when focus changes on the
     *     UrlBar.
     * @param textChangeListener The listener for text changes.
     * @param richTextChangeListener The listener for rich text changes.
     * @param keyDownListener The listener for key down events.
     */
    public UrlBarMediator(
            Context context,
            PropertyModel model,
            Callback<Boolean> focusChangeCallback,
            @Nullable Callback<String> textChangeListener,
            @Nullable Callback<UrlBarTextChangeInfo> richTextChangeListener,
            @Nullable OnKeyListener keyDownListener) {
        mContext = context;
        mModel = model;
        mOnFocusChangeCallback = focusChangeCallback;
        mTextChangeListener = textChangeListener;
        mRichTextChangeListener = richTextChangeListener;

        mModel.set(UrlBarProperties.FOCUS_CHANGE_CALLBACK, this::onUrlFocusChange);
        mModel.set(UrlBarProperties.SHOW_CURSOR, false);
        mModel.set(UrlBarProperties.TEXT_CONTEXT_MENU_DELEGATE, this);
        mModel.set(UrlBarProperties.HAS_URL_SUGGESTIONS, false);
        mModel.set(UrlBarProperties.TEXT_CHANGE_LISTENER, this::onTextChanged);
        mModel.set(UrlBarProperties.RICH_TEXT_CHANGE_LISTENER, this::onRichTextChanged);
        mModel.set(UrlBarProperties.KEY_DOWN_LISTENER, keyDownListener);
        mModel.set(UrlBarProperties.SHOW_HINT_TEXT, true);
        if (OmniboxFeatures.sOmniboxSiteSearch.isEnabled()) {
            mModel.set(
                    UrlBarProperties.MANAGE_SEARCH_ENGINES_CALLBACK,
                    this::onManageSiteSearchClicked);
        }
        setBrandedColorScheme(BrandedColorScheme.APP_DEFAULT);
        pushTextToModel(/* originChanged= */ false);
    }

    public void destroy() {
        mModel.set(UrlBarProperties.FOCUS_CHANGE_CALLBACK, null);
        mModel.set(UrlBarProperties.TEXT_CONTEXT_MENU_DELEGATE, null);
        mModel.set(UrlBarProperties.TEXT_CHANGE_LISTENER, null);
        mModel.set(UrlBarProperties.MANAGE_SEARCH_ENGINES_CALLBACK, null);
    }

    private void onTextChanged(String text) {
        if (mTextChangeListener != null) {
            mTextChangeListener.onResult(text);
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
        boolean showHintText = !mHasFocus || text.isEmpty();
        mModel.set(UrlBarProperties.SHOW_HINT_TEXT, showHintText);
    }

    private void onManageSiteSearchClicked() {
        SettingsNavigationFactory.createSettingsNavigation()
                .startSettings(mContext, SiteSearchSettings.class);
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

        if (!mHasFocus
                && isNewTextEquivalentToExistingText(mUrlBarData, data)
                && mScrollType == scrollType) {
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

    private void pushTextToModel(boolean originChanged) {
        CharSequence text;
        if (mShowOriginOnly && mUrlBarData.originStartIndex != mUrlBarData.originEndIndex) {
            text =
                    mUrlBarData.displayText.subSequence(
                            mUrlBarData.originStartIndex, mUrlBarData.originEndIndex);
        } else {
            text = !mHasFocus ? mUrlBarData.displayText : mUrlBarData.getEditingOrDisplayText();
        }
        CharSequence textForAutofillServices = text;

        if (!(mHasFocus || TextUtils.isEmpty(text) || mUrlBarData.url == null)) {
            textForAutofillServices = mUrlBarData.url.getSpec();
        }

        @ScrollType int scrollType = mHasFocus ? ScrollType.NO_SCROLL : mScrollType;
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
        // equality.  Internally, TextView applies many additional spans that need to be
        // ignored for this comparison to be useful, so this is scoped to only the span types
        // applied by our UI.
        if (!(newCharSequence instanceof Spanned) || !(existingCharSequence instanceof Spanned)) {
            return false;
        }

        Spanned currentText = (Spanned) existingCharSequence;
        Spanned newText = (Spanned) newCharSequence;
        UrlEmphasisSpan[] currentSpans =
                currentText.getSpans(0, currentText.length(), UrlEmphasisSpan.class);
        UrlEmphasisSpan[] newSpans = newText.getSpans(0, newText.length(), UrlEmphasisSpan.class);
        if (currentSpans.length != newSpans.length) return false;
        for (int i = 0; i < currentSpans.length; i++) {
            UrlEmphasisSpan currentSpan = currentSpans[i];
            UrlEmphasisSpan newSpan = newSpans[i];
            if (!currentSpan.equals(newSpan)
                    || currentText.getSpanStart(currentSpan) != newText.getSpanStart(newSpan)
                    || currentText.getSpanEnd(currentSpan) != newText.getSpanEnd(newSpan)
                    || currentText.getSpanFlags(currentSpan) != newText.getSpanFlags(newSpan)) {
                return false;
            }
        }
        return true;
    }

    /**
     * Sets the autocomplete text to be shown.
     *
     * @param userText The existing user text.
     * @param autocompleteText The text to be appended to the user text.
     * @param additionalText This string is displayed adjacent to the omnibox if this match is the
     *     default. Will usually be URL when autocompleting a title, and empty otherwise.
     */
    public void setAutocompleteText(
            String userText,
            @Nullable String autocompleteText,
            @Nullable String additionalText,
            @Nullable String siteSearchLabel) {
        if (!mHasFocus) {
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

    void onUrlFocusChange(boolean focus) {
        if (mIsReparenting) return;
        mHasFocus = focus;

        if (mModel.get(UrlBarProperties.ALLOW_FOCUS)) {
            mModel.set(UrlBarProperties.SHOW_CURSOR, mHasFocus);
        }

        UrlBarTextState preCallbackState = mModel.get(UrlBarProperties.TEXT_STATE);
        mOnFocusChangeCallback.onResult(focus);
        boolean textChangedInFocusCallback =
                mModel.get(UrlBarProperties.TEXT_STATE) != preCallbackState;
        if (!textChangedInFocusCallback) {
            pushTextToModel(/* originChanged= */ false);
        }
        updateShowHintText(mUrlBarData.displayText.toString());
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
        if (allowFocus) {
            mModel.set(UrlBarProperties.SHOW_CURSOR, mHasFocus);
        }
    }

    /**
     * Sets whether the view should *permit* multiline input.
     *
     * <p>The perimitted/allowed wrapping doesn't imply the wrapping will be applied. Only eligible
     * input in focused state can wrap. This setting controls only whether wrapping is permitted.
     */
    public void setAllowMultilineInput(boolean allowMultilineInput) {
        mModel.set(UrlBarProperties.ALLOW_MULTILINE_INPUT, allowMultilineInput);
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
        if (delegate != null) {
            String replacement = delegate.getReplacementCutCopyText(currentText, selection);
            if (replacement != null) return replacement;
        }

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
        Context context = ContextUtils.getApplicationContext();

        ClipboardManager clipboard =
                (ClipboardManager) context.getSystemService(Context.CLIPBOARD_SERVICE);
        ClipData clipData = clipboard.getPrimaryClip();
        if (clipData == null) return null;

        StringBuilder builder = new StringBuilder();
        for (int i = 0; i < clipData.getItemCount(); i++) {
            builder.append(clipData.getItemAt(i).coerceToText(context));
        }

        String stringToPaste = sanitizeTextForPaste(builder.toString());
        return stringToPaste;
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
    void setUrlBarHintText(String hintText) {
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

    void startReparenting() {
        mIsReparenting = true;
    }

    void finishReparenting() {
        mIsReparenting = false;
    }
}
