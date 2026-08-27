// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.fusebox;

import android.widget.ImageView;

import androidx.annotation.IntDef;
import androidx.constraintlayout.widget.ConstraintLayout;
import androidx.recyclerview.widget.RecyclerView;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.omnibox.R;
import org.chromium.ui.widget.ButtonCompat;

import java.lang.annotation.ElementType;
import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.lang.annotation.Target;

/** A ViewHolder for the Fusebox component. */
@NullMarked
class FuseboxViewHolder {
    @IntDef({
        AnchoringMode.UNSET,
        AnchoringMode.POPOVER,
        AnchoringMode.TOOLBAR_SINGLE_LINE,
        AnchoringMode.TOOLBAR_MULTI_LINE,
    })
    @Retention(RetentionPolicy.SOURCE)
    @Target(ElementType.TYPE_USE)
    public @interface AnchoringMode {
        int UNSET = 0;
        int POPOVER = 1;
        int TOOLBAR_SINGLE_LINE = 2;
        int TOOLBAR_MULTI_LINE = 3;
    }

    public final ConstraintLayout parentView;
    public final RecyclerView attachmentsView;
    public final ImageView plusButton;
    public final FuseboxPopup popup;
    public final ButtonCompat requestType;
    public final ImageView navigateButton;

    public @AnchoringMode int currentAnchoringMode = AnchoringMode.UNSET;

    FuseboxViewHolder(ConstraintLayout parent, FuseboxPopup popup) {
        parentView = parent;
        attachmentsView = parent.findViewById(R.id.location_bar_attachments);
        plusButton = parent.findViewById(R.id.fusebox_plus_button);
        requestType = parent.findViewById(R.id.fusebox_request_type);
        navigateButton = parent.findViewById(R.id.navigate_button);
        this.popup = popup;
    }
}
