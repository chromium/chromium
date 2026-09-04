// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.graphics.drawable.Drawable;
import android.os.Build;
import android.text.TextUtils;
import android.view.ActionMode;

import androidx.annotation.ColorInt;
import androidx.constraintlayout.widget.ConstraintLayout;
import androidx.constraintlayout.widget.ConstraintLayout.LayoutParams;

import com.google.android.material.color.MaterialColors;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.omnibox.UrlBarProperties.AutocompleteText;
import org.chromium.chrome.browser.omnibox.UrlBarProperties.UrlBarTextState;
import org.chromium.components.omnibox.TextSelection;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/** Handles translating the UrlBar model data to the view state. */
@NullMarked
class UrlBarViewBinder {
    /**
     * @see PropertyModelChangeProcessor.ViewBinder#bind(Object, Object, Object)
     */
    public static void bind(PropertyModel model, UrlBar view, PropertyKey propertyKey) {
        if (propertyKey == UrlBarProperties.ACCESSIBILITY_WARNING) {
            view.setAccessibilityWarning(model.get(UrlBarProperties.ACCESSIBILITY_WARNING));
        } else if (propertyKey == UrlBarProperties.ACTION_MODE_CALLBACK) {
            ActionMode.Callback callback = model.get(UrlBarProperties.ACTION_MODE_CALLBACK);
            if (callback == null && view.getCustomSelectionActionModeCallback() == null) return;
            view.setCustomSelectionActionModeCallback(callback);
        } else if (propertyKey == UrlBarProperties.AI_MODE_PREF_ENABLED) {
            view.setShowAiMode(model.get(UrlBarProperties.AI_MODE_PREF_ENABLED));
        } else if (propertyKey == UrlBarProperties.AI_MODE_PREF_TOGGLE_CALLBACK) {
            view.setShowAiModeCallback(model.get(UrlBarProperties.AI_MODE_PREF_TOGGLE_CALLBACK));
        } else if (propertyKey == UrlBarProperties.ALLOW_FOCUS) {
            view.setAllowFocus(model.get(UrlBarProperties.ALLOW_FOCUS));
        } else if (propertyKey == UrlBarProperties.ALLOW_MULTILINE_INPUT) {
            view.setAllowMultilineInput(model.get(UrlBarProperties.ALLOW_MULTILINE_INPUT));
        } else if (propertyKey == UrlBarProperties.AUTOCOMPLETE_TEXT) {
            AutocompleteText autocomplete = model.get(UrlBarProperties.AUTOCOMPLETE_TEXT);
            boolean shouldAutocomplete = view.shouldAutocomplete();
            view.setAutocompleteText(
                    autocomplete.userText,
                    shouldAutocomplete ? autocomplete.autocompleteText : null,
                    shouldAutocomplete && !TextUtils.isEmpty(autocomplete.additionalText)
                            ? autocomplete.additionalText
                            : null,
                    autocomplete.siteSearchLabel);
        } else if (propertyKey == UrlBarProperties.DELEGATE) {
            view.setDelegate(model.get(UrlBarProperties.DELEGATE));
        } else if (propertyKey == UrlBarProperties.FOCUS_CHANGE_CALLBACK) {
            view.setFocusChangeCallback(model.get(UrlBarProperties.FOCUS_CHANGE_CALLBACK));
        } else if (propertyKey == UrlBarProperties.HAS_URL_SUGGESTIONS) {
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.UPSIDE_DOWN_CAKE) {
                view.setHandwritingBoundsOffsets(
                        view.getHandwritingBoundsOffsetLeft(),
                        view.getHandwritingBoundsOffsetTop(),
                        view.getHandwritingBoundsOffsetRight(),
                        model.get(UrlBarProperties.HAS_URL_SUGGESTIONS)
                                ? view.getHandwritingBoundsOffsetTop()
                                : 0);
            }
        } else if (propertyKey == UrlBarProperties.HINT_TEXT) {
            view.setHint(getHintForModelState(model));
        } else if (propertyKey == UrlBarProperties.HINT_TEXT_COLOR) {
            view.setHintTextColor(model.get(UrlBarProperties.HINT_TEXT_COLOR));
        } else if (propertyKey == UrlBarProperties.INCOGNITO_COLORS_ENABLED) {
            final boolean incognitoColorsEnabled =
                    model.get(UrlBarProperties.INCOGNITO_COLORS_ENABLED);
            updateHighlightColor(view, incognitoColorsEnabled);
            updateCursorAndSelectHandleColor(view, incognitoColorsEnabled);
        } else if (propertyKey == UrlBarProperties.KEY_DOWN_LISTENER) {
            view.setKeyDownListener(model.get(UrlBarProperties.KEY_DOWN_LISTENER));
        } else if (propertyKey == UrlBarProperties.LONG_CLICK_LISTENER) {
            view.setOnLongClickListener(model.get(UrlBarProperties.LONG_CLICK_LISTENER));
        } else if (propertyKey == UrlBarProperties.MANAGE_SEARCH_ENGINES_CALLBACK) {
            view.setManageSearchEnginesCallback(
                    model.get(UrlBarProperties.MANAGE_SEARCH_ENGINES_CALLBACK));
        } else if (propertyKey == UrlBarProperties.RICH_TEXT_CHANGE_LISTENER) {
            view.setRichTextChangeListener(model.get(UrlBarProperties.RICH_TEXT_CHANGE_LISTENER));
        } else if (propertyKey == UrlBarProperties.SELECT_ALL_ON_FOCUS) {
            view.setSelectAllOnFocus(model.get(UrlBarProperties.SELECT_ALL_ON_FOCUS));
        } else if (propertyKey == UrlBarProperties.SHOW_HINT_TEXT) {
            view.setHint(getHintForModelState(model));
        } else if (propertyKey == UrlBarProperties.TEXT_CHANGE_LISTENER) {
            view.setTextChangeListener(model.get(UrlBarProperties.TEXT_CHANGE_LISTENER));
        } else if (propertyKey == UrlBarProperties.TEXT_COLOR) {
            view.setTextColor(model.get(UrlBarProperties.TEXT_COLOR));
        } else if (propertyKey == UrlBarProperties.TEXT_CONTEXT_MENU_DELEGATE) {
            view.setTextContextMenuDelegate(model.get(UrlBarProperties.TEXT_CONTEXT_MENU_DELEGATE));
        } else if (propertyKey == UrlBarProperties.TEXT_STATE) {
            UrlBarTextState state = model.get(UrlBarProperties.TEXT_STATE);
            view.setIgnoreTextChangesForAutocomplete(true);
            view.setTextWithTruncation(state.text, state.scrollType, state.scrollToIndex);
            view.setTextForAutofillServices(state.textForAutofillServices);
            view.setScrollState(state.scrollType, state.scrollToIndex, state.originChanged);
            view.setIgnoreTextChangesForAutocomplete(false);
            if (view.hasFocus()) {
                // NOTE: Selection applied from here MAY be overridden by the OS if the focus came
                // from the user (and not from software, i.e. requestFocus()).
                //
                // When the user focuses the editable field, the OS forcibly takes one of the two
                // actions:
                // 1. forcibly places the cursor at the point of click, or
                // 2. selecting all content (if selectAllOnFocus is set to true)
                // in both cases overriding the selection supplied by software.
                //
                // Be careful when extending selection to override OS settings - Android 12 is
                // particularly sensitive here.
                int textLength = view.getText().length();
                TextSelection selection = state.selection.trimTo(textLength);
                view.setSelection(selection.from, selection.to);
                view.requestAccessibilityFocus();
            }
        } else if (propertyKey == UrlBarProperties.TEXT_WRAPPED_CALLBACK) {
            view.setUrlTextWrappingChangeListener(
                    model.get(UrlBarProperties.TEXT_WRAPPED_CALLBACK));
        } else if (propertyKey == UrlBarProperties.URL_DIRECTION_LISTENER) {
            view.setUrlDirectionListener(model.get(UrlBarProperties.URL_DIRECTION_LISTENER));
        } else if (propertyKey == UrlBarProperties.USE_SMALL_TEXT) {
            boolean useSmallText = model.get(UrlBarProperties.USE_SMALL_TEXT);
            // Small text mode is used in a state where available vertical space is much lower and
            // there is no location bar "pill" that we must draw inside. Removing the padding avoids
            // over-constraining the text size to the point of illegibility.
            int verticalPadding =
                    useSmallText
                            ? 0
                            : view.getResources()
                                    .getDimensionPixelSize(R.dimen.url_bar_vertical_padding);
            view.setPaddingRelative(
                    view.getPaddingStart(), verticalPadding, view.getPaddingEnd(), verticalPadding);
            view.setUseSmallTextHeight(useSmallText);
            view.setHint(getHintForModelState(model));
            ConstraintLayout.LayoutParams layoutParams =
                    (ConstraintLayout.LayoutParams) view.getLayoutParams();
            layoutParams.width =
                    useSmallText ? LayoutParams.WRAP_CONTENT : LayoutParams.MATCH_CONSTRAINT;
        }
    }

    private static void updateHighlightColor(UrlBar view, boolean useIncognitoColors) {
        @ColorInt int originalHighlightColor;
        Object highlightColorObj = view.getTag(R.id.highlight_color);
        if (highlightColorObj == null || !(highlightColorObj instanceof Integer)) {
            originalHighlightColor = view.getHighlightColor();
            view.setTag(R.id.highlight_color, originalHighlightColor);
        } else {
            originalHighlightColor = (Integer) highlightColorObj;
        }

        int highlightColor;
        if (useIncognitoColors) {
            highlightColor = view.getContext().getColor(R.color.text_highlight_color_incognito);
        } else {
            highlightColor = originalHighlightColor;
        }

        view.setHighlightColor(highlightColor);
    }

    private static void updateCursorAndSelectHandleColor(UrlBar view, boolean useIncognitoColors) {
        // These get* methods may fail on some devices, so we're calling all of them before
        // applying tint to any of the drawables. See https://crbug.com/40800314.
        final Drawable textCursor = assumeNonNull(view.getTextCursorDrawable());
        final Drawable textSelectHandle = assumeNonNull(view.getTextSelectHandle());
        final Drawable textSelectHandleLeft = assumeNonNull(view.getTextSelectHandleLeft());
        final Drawable textSelectHandleRight = assumeNonNull(view.getTextSelectHandleRight());

        final @ColorInt int color =
                useIncognitoColors
                        ? view.getContext().getColor(R.color.default_control_color_active_dark)
                        : MaterialColors.getColor(view, R.attr.colorPrimary);
        textCursor.mutate().setTint(color);
        textSelectHandle.mutate().setTint(color);
        textSelectHandleLeft.mutate().setTint(color);
        textSelectHandleRight.mutate().setTint(color);
    }

    private static @Nullable CharSequence getHintForModelState(PropertyModel model) {
        // Android TextViews set a desired size based on the max of the hint text size and the
        // "regular" size. In small text mode, where we don't intend to show the hint, we set it to
        // null to avoid over-allocating space for text that will never be shown.
        // Similarly, we set SHOW_HINT_TEXT to false in other cases when we don't intend to show the
        // hint and wish to avoid over-allocating space, e.g. when entering text in the focused
        // state where the hint could cause premature wrapping to another line.
        return model.get(UrlBarProperties.USE_SMALL_TEXT)
                        || !model.get(UrlBarProperties.SHOW_HINT_TEXT)
                ? null
                : model.get(UrlBarProperties.HINT_TEXT);
    }

    private UrlBarViewBinder() {}
}
