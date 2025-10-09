// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.navattach;

import android.view.ViewGroup;

import androidx.appcompat.widget.SwitchCompat;
import androidx.constraintlayout.widget.Group;
import androidx.recyclerview.widget.RecyclerView;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.omnibox.R;
import org.chromium.ui.widget.ChromeImageButton;

/** A ViewHolder for the NavigationAttachments component. */
@NullMarked
class NavigationAttachmentsViewHolder {
    public final RecyclerView attachmentsView;
    public final Group attachmentsToolbar;
    public final ChromeImageButton addButton;
    public final ChromeImageButton settingsButton;
    public final Group navigationTypeGroup;
    public final SwitchCompat navigationType;
    public final NavigationAttachmentsPopup popup;

    NavigationAttachmentsViewHolder(ViewGroup parent, NavigationAttachmentsPopup popup) {
        attachmentsView = parent.findViewById(R.id.location_bar_attachments);
        attachmentsToolbar = parent.findViewById(R.id.location_bar_attachments_toolbar);
        addButton = parent.findViewById(R.id.location_bar_attachments_add);
        settingsButton = parent.findViewById(R.id.location_bar_attachments_settings);
        navigationTypeGroup = parent.findViewById(R.id.location_bar_navigation_type_group);
        navigationType = parent.findViewById(R.id.location_bar_navigation_type);
        this.popup = popup;
    }
}
