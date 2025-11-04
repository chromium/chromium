// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.fusebox;

import android.view.ViewGroup;

import androidx.constraintlayout.widget.Group;
import androidx.recyclerview.widget.RecyclerView;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.omnibox.R;
import org.chromium.components.browser_ui.widget.RoundedCornerOutlineProvider;
import org.chromium.ui.widget.ButtonCompat;
import org.chromium.ui.widget.ChromeImageView;

/** A ViewHolder for the NavigationAttachments component. */
@NullMarked
class NavigationAttachmentsViewHolder {
    public final ViewGroup parentView;
    public final RecyclerView attachmentsView;
    public final Group attachmentsToolbar;
    public final ChromeImageView addButton;
    public final ChromeImageView settingsButton;
    public final NavigationAttachmentsPopup popup;
    public final ButtonCompat requestType;
    public final ChromeImageView navigateButton;

    NavigationAttachmentsViewHolder(ViewGroup parent, NavigationAttachmentsPopup popup) {
        parentView = parent;
        attachmentsView = parent.findViewById(R.id.location_bar_attachments);
        attachmentsToolbar = parent.findViewById(R.id.location_bar_attachments_toolbar);
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
