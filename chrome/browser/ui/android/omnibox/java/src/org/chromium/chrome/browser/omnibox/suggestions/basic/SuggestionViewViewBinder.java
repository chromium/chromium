// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions.basic;

import android.text.TextUtils;
import android.view.View;
import android.widget.TextView;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.omnibox.R;
import org.chromium.chrome.browser.omnibox.styles.SuggestionSpannable;
import org.chromium.chrome.browser.omnibox.suggestions.base.BaseSuggestionViewBinder;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/** Properties associated with the basic suggestion view. */
@NullMarked
public class SuggestionViewViewBinder extends BaseSuggestionViewBinder<View> {
    private static class ViewHolder {
        final TextView mLine1;
        final TextView mLine2;

        ViewHolder(View contentView) {
            mLine1 = contentView.findViewById(R.id.line_1);
            mLine2 = contentView.findViewById(R.id.line_2);
        }
    }

    private static ViewHolder getViewHolder(View view) {
        ViewHolder holder = (ViewHolder) view.getTag();
        if (holder == null) {
            holder = new ViewHolder(view);
            view.setTag(holder);
        }
        return holder;
    }

    /**
     * @see PropertyModelChangeProcessor.ViewBinder#bind(Object, Object, Object)
     */
    @Override
    protected void bindContent(PropertyModel model, View view, PropertyKey propertyKey) {
        if (propertyKey == SuggestionViewProperties.CONTENT_DESCRIPTION) {
            view.setContentDescription(model.get(SuggestionViewProperties.CONTENT_DESCRIPTION));
        } else if (propertyKey == SuggestionViewProperties.IS_SEARCH_SUGGESTION) {
            // https://crbug.com/40084252: ensure URLs are always composed LTR and that their
            // components are not re-ordered.
            final boolean isSearch = model.get(SuggestionViewProperties.IS_SEARCH_SUGGESTION);
            final TextView tv = getViewHolder(view).mLine2;
            tv.setTextDirection(
                    isSearch ? TextView.TEXT_DIRECTION_INHERIT : TextView.TEXT_DIRECTION_LTR);
        } else if (propertyKey == SuggestionViewProperties.TEXT_LINE_1_TEXT) {
            getViewHolder(view)
                    .mLine1
                    .setText(model.get(SuggestionViewProperties.TEXT_LINE_1_TEXT));
        } else if (propertyKey == SuggestionViewProperties.TEXT_LINE_1_WRAP) {
            updateMaxLines(
                    getViewHolder(view).mLine1,
                    model.get(SuggestionViewProperties.TEXT_LINE_1_WRAP));
        } else if (propertyKey == SuggestionViewProperties.TEXT_LINE_2_TEXT) {
            bindTextLine2Text(model, view);
        } else if (propertyKey == SuggestionViewProperties.TEXT_LINE_2_WRAP) {
            updateMaxLines(
                    getViewHolder(view).mLine2,
                    model.get(SuggestionViewProperties.TEXT_LINE_2_WRAP));
        }
    }

    private void bindTextLine2Text(PropertyModel model, View view) {
        TextView tv = getViewHolder(view).mLine2;
        final SuggestionSpannable span = model.get(SuggestionViewProperties.TEXT_LINE_2_TEXT);
        final int minHeight;
        if (!TextUtils.isEmpty(span)) {
            tv.setText(span);
            tv.setVisibility(View.VISIBLE);
            minHeight = getResourceProvider(model).getSuggestionMinHeight(2);
        } else {
            tv.setVisibility(View.GONE);
            minHeight = getResourceProvider(model).getSuggestionMinHeight(1);
        }
        if (view.getMinimumHeight() != minHeight) {
            view.setMinimumHeight(minHeight);
        }
    }

    private void updateMaxLines(TextView tv, boolean allowWrapAround) {
        int maxLines = allowWrapAround ? 2 : 1;
        if (tv.getMaxLines() != maxLines) {
            tv.setMaxLines(maxLines);
        }
    }
}
