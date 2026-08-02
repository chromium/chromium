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

    /**
     * @see PropertyModelChangeProcessor.ViewBinder#bind(Object, Object, Object)
     */
    @Override
    protected void bindContent(PropertyModel model, View view, PropertyKey propertyKey) {
        if (propertyKey == SuggestionViewProperties.TEXT_LINE_1_TEXT_APPEARANCE) {
            TextView tv = view.findViewById(R.id.line_1);
            tv.setTextAppearance(model.get(SuggestionViewProperties.TEXT_LINE_1_TEXT_APPEARANCE));
        } else if (propertyKey == SuggestionViewProperties.TEXT_LINE_1_TEXT) {
            TextView tv = view.findViewById(R.id.line_1);
            tv.setText(model.get(SuggestionViewProperties.TEXT_LINE_1_TEXT));
            int minHeight = getResourceProvider(model).getSuggestionMinHeight(tv.getLineCount());
            view.setMinimumHeight(minHeight);
        } else if (propertyKey == SuggestionViewProperties.IS_SEARCH_SUGGESTION) {
            // https://crbug.com/40084252: ensure URLs are always composed LTR and that their
            // components are not re-ordered.
            final boolean isSearch = model.get(SuggestionViewProperties.IS_SEARCH_SUGGESTION);
            final TextView tv = view.findViewById(R.id.line_2);
            tv.setTextDirection(
                    isSearch ? TextView.TEXT_DIRECTION_INHERIT : TextView.TEXT_DIRECTION_LTR);
        } else if (propertyKey == SuggestionViewProperties.TEXT_LINE_2_TEXT) {
            TextView tv = view.findViewById(R.id.line_2);
            final SuggestionSpannable span = model.get(SuggestionViewProperties.TEXT_LINE_2_TEXT);
            if (!TextUtils.isEmpty(span)) {
                tv.setText(span);
                tv.setVisibility(View.VISIBLE);
                view.setMinimumHeight(getResourceProvider(model).getSuggestionMinHeight(2));
            } else {
                tv.setVisibility(View.GONE);
                view.setMinimumHeight(getResourceProvider(model).getSuggestionMinHeight(1));
            }
        } else if (propertyKey == SuggestionViewProperties.ALLOW_WRAP_AROUND) {
            final boolean allowWrapAround = model.get(SuggestionViewProperties.ALLOW_WRAP_AROUND);
            TextView tv = view.findViewById(R.id.line_1);
            int maxLines = allowWrapAround ? 2 : 1;
            if (tv.getMaxLines() != maxLines) {
                tv.setMaxLines(maxLines);
            }
        } else if (propertyKey == SuggestionViewProperties.CONTENT_DESCRIPTION) {
            view.setContentDescription(model.get(SuggestionViewProperties.CONTENT_DESCRIPTION));
        }
    }
}
