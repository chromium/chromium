// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.extensions;

import android.content.Context;
import android.util.AttributeSet;

import androidx.recyclerview.widget.RecyclerView;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

/**
 * A {@link RecyclerView} for extension action buttons. This container will automatically hide any
 * buttons that don't fit into the allowed width.
 */
@NullMarked
public class ExtensionActionListRecyclerView extends RecyclerView {

    public ExtensionActionListRecyclerView(Context context) {
        super(context);
    }

    public ExtensionActionListRecyclerView(Context context, @Nullable AttributeSet attrs) {
        super(context, attrs);
    }

    public ExtensionActionListRecyclerView(
            Context context, @Nullable AttributeSet attrs, int defStyle) {
        super(context, attrs, defStyle);
    }
}
