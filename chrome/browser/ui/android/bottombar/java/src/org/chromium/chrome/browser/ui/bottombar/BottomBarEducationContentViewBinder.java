// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.bottombar;

import static org.chromium.chrome.browser.ui.bottombar.BottomBarEducationContentProperties.BODY;
import static org.chromium.chrome.browser.ui.bottombar.BottomBarEducationContentProperties.ILLUSTRATION;
import static org.chromium.chrome.browser.ui.bottombar.BottomBarEducationContentProperties.IS_BUTTON_BAR_VISIBLE;
import static org.chromium.chrome.browser.ui.bottombar.BottomBarEducationContentProperties.IS_ILLUSTRATION_VISIBLE;
import static org.chromium.chrome.browser.ui.bottombar.BottomBarEducationContentProperties.ON_NEGATIVE_CLICKED;
import static org.chromium.chrome.browser.ui.bottombar.BottomBarEducationContentProperties.ON_POSITIVE_CLICKED;
import static org.chromium.chrome.browser.ui.bottombar.BottomBarEducationContentProperties.TITLE;

import android.text.method.LinkMovementMethod;
import android.view.View;
import android.widget.ImageView;
import android.widget.TextView;

import org.chromium.build.annotations.NullMarked;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/** Binds {@link BottomBarEducationContentProperties} to {@code bottom_bar_education_content}. */
@NullMarked
class BottomBarEducationContentViewBinder {
    private BottomBarEducationContentViewBinder() {}

    static void bind(PropertyModel model, View view, PropertyKey propertyKey) {
        if (ILLUSTRATION == propertyKey) {
            ImageView illustration = view.findViewById(R.id.education_illustration);
            illustration.setImageDrawable(model.get(ILLUSTRATION));
        } else if (IS_ILLUSTRATION_VISIBLE == propertyKey) {
            view.findViewById(R.id.education_illustration)
                    .setVisibility(model.get(IS_ILLUSTRATION_VISIBLE) ? View.VISIBLE : View.GONE);
        } else if (TITLE == propertyKey) {
            TextView title = view.findViewById(R.id.education_title);
            title.setText(model.get(TITLE));
        } else if (BODY == propertyKey) {
            TextView body = view.findViewById(R.id.education_body);
            body.setText(model.get(BODY));
            body.setMovementMethod(LinkMovementMethod.getInstance());
        } else if (IS_BUTTON_BAR_VISIBLE == propertyKey) {
            view.findViewById(R.id.button_bar)
                    .setVisibility(model.get(IS_BUTTON_BAR_VISIBLE) ? View.VISIBLE : View.GONE);
        } else if (ON_POSITIVE_CLICKED == propertyKey) {
            Runnable onPositiveClicked = model.get(ON_POSITIVE_CLICKED);
            view.findViewById(R.id.positive_button)
                    .setOnClickListener(_ -> onPositiveClicked.run());
        } else if (ON_NEGATIVE_CLICKED == propertyKey) {
            Runnable onNegativeClicked = model.get(ON_NEGATIVE_CLICKED);
            view.findViewById(R.id.negative_button)
                    .setOnClickListener(_ -> onNegativeClicked.run());
        }
    }
}
