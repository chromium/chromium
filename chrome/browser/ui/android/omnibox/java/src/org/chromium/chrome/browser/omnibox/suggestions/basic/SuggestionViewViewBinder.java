// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions.basic;

import android.text.Spanned;
import android.text.TextUtils;
import android.view.View;
import android.widget.TextView;

import androidx.annotation.VisibleForTesting;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.omnibox.styles.SuggestionSpannable;
import org.chromium.chrome.browser.omnibox.suggestions.base.BaseSuggestionViewBinder;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/** Properties associated with the basic suggestion view. */
@NullMarked
public class SuggestionViewViewBinder extends BaseSuggestionViewBinder<TwoLineSuggestionView> {
    /**
     * @see PropertyModelChangeProcessor.ViewBinder#bind(Object, Object, Object)
     */
    @Override
    protected void bindContent(
            PropertyModel model, TwoLineSuggestionView view, PropertyKey propertyKey) {
        if (propertyKey == SuggestionViewProperties.CONTENT_DESCRIPTION) {
            view.setContentDescription(model.get(SuggestionViewProperties.CONTENT_DESCRIPTION));
        } else if (propertyKey == SuggestionViewProperties.IS_SEARCH_SUGGESTION) {
            // https://crbug.com/40084252: ensure URLs are always composed LTR and that their
            // components are not re-ordered.
            final boolean isSearch = model.get(SuggestionViewProperties.IS_SEARCH_SUGGESTION);
            view.getLine2()
                    .setTextDirection(
                            isSearch
                                    ? TextView.TEXT_DIRECTION_INHERIT
                                    : TextView.TEXT_DIRECTION_LTR);
        } else if (propertyKey == SuggestionViewProperties.TEXT_LINE_1_TEXT) {
            setTextIfChanged(view.getLine1(), model.get(SuggestionViewProperties.TEXT_LINE_1_TEXT));
        } else if (propertyKey == SuggestionViewProperties.TEXT_LINE_1_WRAP) {
            updateMaxLines(view.getLine1(), model.get(SuggestionViewProperties.TEXT_LINE_1_WRAP));
        } else if (propertyKey == SuggestionViewProperties.TEXT_LINE_2_TEXT) {
            bindTextLine2Text(model, view);
        } else if (propertyKey == SuggestionViewProperties.TEXT_LINE_2_WRAP) {
            updateMaxLines(view.getLine2(), model.get(SuggestionViewProperties.TEXT_LINE_2_WRAP));
        }
    }

    private void bindTextLine2Text(PropertyModel model, TwoLineSuggestionView view) {
        TextView line2 = view.getLine2();
        final SuggestionSpannable span = model.get(SuggestionViewProperties.TEXT_LINE_2_TEXT);
        final int minHeight;
        if (!TextUtils.isEmpty(span)) {
            setTextIfChanged(line2, span);
            line2.setVisibility(View.VISIBLE);
            minHeight = getResourceProvider(model).getSuggestionMinHeight(2);
        } else {
            line2.setVisibility(View.GONE);
            minHeight = getResourceProvider(model).getSuggestionMinHeight(1);
        }
        if (view.getMinimumHeight() != minHeight) {
            view.setMinimumHeight(minHeight);
        }
    }

    /**
     * Apply {@code text} to {@code tv} unless the view already shows it. TextView never
     * short-circuits identical text (every setText copies the spannable and re-measures), and the
     * suggestion list is currently rebuilt wholesale several times per keystroke, so most rebinds
     * of a recycled view carry the same text it already displays. The comparison is against the
     * TextView's own copy of the previous text (which preserves span order), so there is no cached
     * state that could go stale.
     *
     * @param tv The TextView being bound.
     * @param text The text to show; null clears the view.
     */
    @VisibleForTesting
    static void setTextIfChanged(TextView tv, @Nullable SuggestionSpannable text) {
        if (text != null
                && tv.getText() instanceof Spanned shown
                && SuggestionSpannable.contentEquals(shown, text)) {
            return;
        }
        tv.setText(text);
    }

    private void updateMaxLines(TextView tv, boolean allowWrapAround) {
        int maxLines = allowWrapAround ? 2 : 1;
        if (tv.getMaxLines() != maxLines) {
            tv.setMaxLines(maxLines);
        }
    }
}
