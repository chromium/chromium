// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.multiwindow;

import android.view.View;
import android.widget.ImageView;
import android.widget.TextView;

import androidx.core.content.ContextCompat;

import org.chromium.build.annotations.NullMarked;
import org.chromium.ui.listmenu.ListMenuButton;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

@NullMarked
class TargetSelectorItemViewBinder {
    public static void bind(PropertyModel model, View view, PropertyKey propertyKey) {
        if (TargetSelectorItemProperties.FAVICON == propertyKey) {
            ((ImageView) view.findViewById(R.id.favicon))
                    .setImageDrawable(model.get(TargetSelectorItemProperties.FAVICON));
        } else if (TargetSelectorItemProperties.IS_SELECTED == propertyKey) {
            ImageView faviconView = view.findViewById(R.id.favicon);
            boolean isSelected = model.get(TargetSelectorItemProperties.IS_SELECTED);

            view.setSelected(isSelected);
            view.findViewById(R.id.title).setSelected(isSelected);
            view.findViewById(R.id.desc).setSelected(isSelected);
            view.findViewById(R.id.last_accessed).setSelected(isSelected);

            // Show check mark if selected, otherwise fallback to favicon.
            faviconView.setImageDrawable(
                    isSelected
                            ? ContextCompat.getDrawable(
                                    view.getContext(), R.drawable.checkmark_circle_24dp)
                            : model.get(TargetSelectorItemProperties.FAVICON));

        } else if (TargetSelectorItemProperties.TITLE == propertyKey) {
            ((TextView) view.findViewById(R.id.title))
                    .setText(model.get(TargetSelectorItemProperties.TITLE));

        } else if (TargetSelectorItemProperties.DESC == propertyKey) {
            ((TextView) view.findViewById(R.id.desc))
                    .setText(model.get(TargetSelectorItemProperties.DESC));

        } else if (TargetSelectorItemProperties.CLICK_LISTENER == propertyKey) {
            view.setOnClickListener(model.get(TargetSelectorItemProperties.CLICK_LISTENER));

        } else if (TargetSelectorItemProperties.CHECK_TARGET == propertyKey) {
            // TODO: Let the talkback relay the checked status.
            boolean visible = model.get(TargetSelectorItemProperties.CHECK_TARGET);
            ImageView checkmark = view.findViewById(R.id.check_mark);
            checkmark.setVisibility(visible ? View.VISIBLE : View.INVISIBLE);
        } else if (TargetSelectorItemProperties.LAST_ACCESSED == propertyKey) {
            TextView lastAccessedView = view.findViewById(R.id.last_accessed);
            String text = model.get(TargetSelectorItemProperties.LAST_ACCESSED);
            lastAccessedView.setText(text);
            ListMenuButton moreButton = view.findViewById(R.id.more);
            moreButton.setVisibility(View.GONE);
        }
    }
}
