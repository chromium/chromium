// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.navattach;

import android.content.Context;
import android.util.AttributeSet;

import androidx.recyclerview.widget.RecyclerView;

import org.chromium.build.annotations.NullMarked;

/** A RecyclerView for the NavigationAttachments component. */
@NullMarked
public class NavigationAttachmentsRecyclerView extends RecyclerView {
    public NavigationAttachmentsRecyclerView(Context context, AttributeSet attrs) {
        super(context, attrs);
    }
}
