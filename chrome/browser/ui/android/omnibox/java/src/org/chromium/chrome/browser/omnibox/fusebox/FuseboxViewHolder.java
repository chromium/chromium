// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.fusebox;

import androidx.constraintlayout.widget.ConstraintLayout;
import androidx.recyclerview.widget.RecyclerView;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.omnibox.R;
import org.chromium.components.browser_ui.widget.RoundedCornerOutlineProvider;
import org.chromium.ui.widget.ButtonCompat;
import org.chromium.ui.widget.ChromeImageView;

/** A ViewHolder for the Fusebox component. */
@NullMarked
class FuseboxViewHolder {
    public final ConstraintLayout parentView;
    public final RecyclerView attachmentsView;
    public final ChromeImageView addButton;
    public final ChromeImageView settingsButton;
    public final FuseboxPopup popup;
    public final ButtonCompat requestType;
    public final ChromeImageView navigateButton;

    FuseboxViewHolder(ConstraintLayout parent, FuseboxPopup popup) {
        parentView = parent;
        attachmentsView = parent.findViewById(R.id.location_bar_attachments);
        addButton = parent.findViewById(R.id.location_bar_attachments_add);
        settingsButton = parent.findViewById(R.id.location_bar_attachments_settings);
        requestType = parent.findViewById(R.id.fusebox_request_type);
        navigateButton = parent.findViewById(R.id.navigate_button);

        this.popup = popup;

        var outline =
                new RoundedCornerOutlineProvider(
                        parent.getResources()
                                .getDimensionPixelSize(R.dimen.fusebox_button_corner_radius));
        outline.setClipPaddedArea(true);
        addButton.setOutlineProvider(outline);
        settingsButton.setOutlineProvider(outline);
    }
}
