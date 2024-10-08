// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox;

import android.graphics.drawable.Drawable;
import android.os.Build;
import android.text.TextUtils;
import android.view.ActionMode;

import androidx.annotation.ColorInt;
import androidx.annotation.RequiresApi;

import com.google.android.material.color.MaterialColors;

import org.chromium.base.Callback;
import org.chromium.chrome.browser.omnibox.UrlBarProperties.AutocompleteText;
import org.chromium.chrome.browser.omnibox.UrlBarProperties.UrlBarTextState;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.Optional;

/** Handles translating the UrlBar model data to the view state. */
class UrlBarViewBinder {
    /**
     * @see PropertyModelChangeProcessor.ViewBinder#bind(Object, Object, Object)
     */
    public static void bind(PropertyModel model, UrlBar view, PropertyKey propertyKey) {
        if (UrlBarProperties.ACTION_MODE_CALLBACK.equals(propertyKey)) {
            ActionMode.Callback callback = model.get(UrlBarProperties.ACTION_MODE_CALLBACK);
            if (callback == null && view.getCustomSelectionActionModeCallback() == null) return;
            view.setCustomSelectionActionModeCallback(callback);
        } else if (UrlBarProperties.ALLOW_FOCUS.equals(propertyKey)) {
            view.setAllowFocus(model.get(UrlBarProperties.ALLOW_FOCUS));
        } else if (UrlBarProperties.AUTOCOMPLETE_TEXT.equals(propertyKey)) {
            AutocompleteText autocomplete = model.get(UrlBarProperties.AUTOCOMPLETE_TEXT);
            if (view.shouldAutocomplete()) {
                view.setAutocompleteText(
                        autocomplete.userText,
                        autocomplete.autocompleteText,
                        TextUtils.isEmpty(autocomplete.additionalText)
                                ? Optional.empty()
                                : Optional.of(autocomplete.additionalText));
            }
        } else if (UrlBarProperties.DELEGATE.equals(propertyKey)) {
            view.setDelegate(model.get(UrlBarProperties.DELEGATE));
        } else if (UrlBarProperties.FOCUS_CHANGE_CALLBACK.equals(propertyKey)) {
            final Optional<Callback<Boolean>> focusChangeCallback =
                    Optional.ofNullable(model.get(UrlBarProperties.FOCUS_CHANGE_CALLBACK));
            view.setOnFocusChangeListener(
                    (v, focused) -> {
                        if (focused) view.setIgnoreTextChangesForAutocomplete(false);
                        focusChangeCallback.ifPresent(cb -> cb.onResult(focused));
                    });
        } else if (UrlBarProperties.SHOW_CURSOR.equals(propertyKey)) {
            view.setCursorVisible(model.get(UrlBarProperties.SHOW_CURSOR));
        } else if (UrlBarProperties.TEXT_CONTEXT_MENU_DELEGATE.equals(propertyKey)) {
            view.setTextContextMenuDelegate(model.get(UrlBarProperties.TEXT_CONTEXT_MENU_DELEGATE));
        } else if (UrlBarProperties.TEXT_STATE.equals(propertyKey)) {
            UrlBarTextState state = model.get(UrlBarProperties.TEXT_STATE);
            view.setIgnoreTextChangesForAutocomplete(true);
            view.setTextWithTruncation(state.text, state.scrollType, state.scrollToIndex);
            view.setTextForAutofillServices(state.textForAutofillServices);
            view.setScrollState(state.scrollType, state.scrollToIndex);
            view.setIgnoreTextChangesForAutocomplete(false);
            if (view.hasFocus()) {
                if (state.selectionState == UrlBarCoordinator.SelectionState.SELECT_ALL) {
                    view.selectAll();
                } else if (state.selectionState == UrlBarCoordinator.SelectionState.SELECT_END) {
                    view.setSelection(view.getText().length());
                }
            }
        } else if (UrlBarProperties.TEXT_COLOR.equals(propertyKey)) {
            view.setTextColor(model.get(UrlBarProperties.TEXT_COLOR));
        } else if (UrlBarProperties.HINT_TEXT_COLOR.equals(propertyKey)) {
            view.setHintTextColor(model.get(UrlBarProperties.HINT_TEXT_COLOR));
        } else if (UrlBarProperties.INCOGNITO_COLORS_ENABLED.equals(propertyKey)) {
            final boolean incognitoColorsEnabled =
                    model.get(UrlBarProperties.INCOGNITO_COLORS_ENABLED);
            updateHighlightColor(view, incognitoColorsEnabled);
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q) {
                updateCursorAndSelectHandleColor(view, incognitoColorsEnabled);
            }
        } else if (UrlBarProperties.URL_DIRECTION_LISTENER.equals(propertyKey)) {
            view.setUrlDirectionListener(model.get(UrlBarProperties.URL_DIRECTION_LISTENER));
        } else if (UrlBarProperties.TEXT_CHANGE_LISTENER.equals(propertyKey)) {
            view.setTextChangeListener(model.get(UrlBarProperties.TEXT_CHANGE_LISTENER));
        } else if (UrlBarProperties.TYPING_STARTED_LISTENER.equals(propertyKey)) {
            view.setTypingStartedListener(model.get(UrlBarProperties.TYPING_STARTED_LISTENER));
        } else if (UrlBarProperties.KEY_DOWN_LISTENER.equals(propertyKey)) {
            view.setKeyDownListener(model.get(UrlBarProperties.KEY_DOWN_LISTENER));
        } else if (UrlBarProperties.WINDOW_DELEGATE.equals(propertyKey)) {
            view.setWindowDelegate(model.get(UrlBarProperties.WINDOW_DELEGATE));
        } else if (UrlBarProperties.HAS_URL_SUGGESTIONS.equals(propertyKey)) {
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.UPSIDE_DOWN_CAKE) {
                view.setHandwritingBoundsOffsets(
                        view.getHandwritingBoundsOffsetLeft(),
                        view.getHandwritingBoundsOffsetTop(),
                        view.getHandwritingBoundsOffsetRight(),
                        model.get(UrlBarProperties.HAS_URL_SUGGESTIONS)
                                ? view.getHandwritingBoundsOffsetTop()
                                : 0);
            }
        } else if (UrlBarProperties.SELECT_ALL_ON_FOCUS.equals(propertyKey)) {
            view.setSelectAllOnFocus(model.get(UrlBarProperties.SELECT_ALL_ON_FOCUS));
        } else if (UrlBarProperties.LONG_CLICK_LISTENER.equals(propertyKey)) {
            view.setOnLongClickListener(model.get(UrlBarProperties.LONG_CLICK_LISTENER));
        } else if (UrlBarProperties.HINT_TEXT.equals(propertyKey)) {
            view.setHint(view.getContext().getString(model.get(UrlBarProperties.HINT_TEXT)));
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

    @RequiresApi(api = Build.VERSION_CODES.Q)
    private static void updateCursorAndSelectHandleColor(UrlBar view, boolean useIncognitoColors) {
        // These get* methods may fail on some devices, so we're calling all of them before
        // applying tint to any of the drawables. See https://crbug.com/1263630.
        final Drawable textCursor = view.getTextCursorDrawable();
        final Drawable textSelectHandle = view.getTextSelectHandle();
        final Drawable textSelectHandleLeft = view.getTextSelectHandleLeft();
        final Drawable textSelectHandleRight = view.getTextSelectHandleRight();

        final int color =
                useIncognitoColors
                        ? view.getContext().getColor(R.color.default_control_color_active_dark)
                        : MaterialColors.getColor(view, R.attr.colorPrimary);
        textCursor.mutate().setTint(color);
        textSelectHandle.mutate().setTint(color);
        textSelectHandleLeft.mutate().setTint(color);
        textSelectHandleRight.mutate().setTint(color);
    }

    private UrlBarViewBinder() {}
}
