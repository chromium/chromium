// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.fusebox;

import android.content.Context;
import android.util.AttributeSet;
import android.view.View;
import android.widget.FrameLayout;
import android.widget.ImageButton;
import android.widget.ImageView;
import android.widget.ProgressBar;
import android.widget.TextView;

import org.chromium.build.annotations.Initializer;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.omnibox.R;

/** View for a single Fusebox attachment item. Caches references to its child views. */
@NullMarked
public class FuseboxAttachmentView extends FrameLayout {
    private View mBackground;
    private ProgressBar mSpinner;
    private ImageView mThumbnail;
    private @Nullable TextView mTitle;
    private ImageButton mRemoveButton;

    public FuseboxAttachmentView(Context context, @Nullable AttributeSet attrs) {
        super(context, attrs);
    }

    @Initializer
    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();
        mBackground = findViewById(R.id.attachment_background);
        mSpinner = findViewById(R.id.attachment_spinner);
        mThumbnail = findViewById(R.id.attachment_thumbnail);
        mTitle = findViewById(R.id.attachment_title);
        mRemoveButton = findViewById(R.id.attachment_remove_button);
    }

    View getBackgroundView() {
        return mBackground;
    }

    ProgressBar getSpinner() {
        return mSpinner;
    }

    ImageView getThumbnail() {
        return mThumbnail;
    }

    @Nullable TextView getTitle() {
        return mTitle;
    }

    ImageButton getRemoveButton() {
        return mRemoveButton;
    }
}
