// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.video_tutorials.iph;

import android.graphics.Bitmap;
import android.graphics.drawable.ColorDrawable;
import android.view.View;
import android.view.ViewStub;
import android.widget.ImageView;
import android.widget.TextView;

import androidx.annotation.Nullable;

import org.chromium.chrome.browser.video_tutorials.R;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * The View component of the video tutorial IPH.  This takes the {@link ViewStub} and inflates it to
 * show the UI.
 */
class VideoIPHView {
    private final ViewStub mViewStub;
    private View mCardView;

    /** Constructor. */
    public VideoIPHView(ViewStub viewStub) {
        mViewStub = viewStub;
    }

    private void setVisibility(boolean visible) {
        if (visible && mCardView == null) mCardView = mViewStub.inflate();
        if (mCardView != null) mCardView.setVisibility(visible ? View.VISIBLE : View.GONE);
    }

    private void setTitle(String title) {
        TextView view = mCardView.findViewById(R.id.title);
        view.setText(title);
    }

    /** Called to set the video length text of an IPH. */
    private void setVideoLength(String videoLength) {
        TextView view = mCardView.findViewById(R.id.video_length);
        view.setText(videoLength);
    }

    private void setThumbnail(@Nullable Bitmap bitmap) {
        ImageView view = mCardView.findViewById(R.id.thumbnail);
        if (bitmap == null) {
            view.setImageDrawable(
                    new ColorDrawable(view.getResources().getColor(R.color.image_loading_color)));
        } else {
            view.setImageBitmap(bitmap);
        }
    }

    private void setClickListener(Runnable clickListener) {
        mCardView.setOnClickListener(v -> clickListener.run());
    }

    private void setDismissListener(Runnable dismissListener) {
        View closeButton = mCardView.findViewById(R.id.close_button);
        closeButton.setOnClickListener(view -> {
            mCardView.setVisibility(View.GONE);
            dismissListener.run();
        });
    }

    /** The view binder that propagates events from model to view. */
    public static void bind(PropertyModel model, VideoIPHView view, PropertyKey propertyKey) {
        if (propertyKey == VideoIPHProperties.VISIBILITY) {
            view.setVisibility(model.get(VideoIPHProperties.VISIBILITY));
        } else if (propertyKey == VideoIPHProperties.DISPLAY_TITLE) {
            view.setTitle(model.get(VideoIPHProperties.DISPLAY_TITLE));
        } else if (propertyKey == VideoIPHProperties.VIDEO_LENGTH) {
            view.setVideoLength(model.get(VideoIPHProperties.VIDEO_LENGTH));
        } else if (propertyKey == VideoIPHProperties.THUMBNAIL) {
            view.setThumbnail(model.get(VideoIPHProperties.THUMBNAIL));
        } else if (propertyKey == VideoIPHProperties.CLICK_LISTENER) {
            view.setClickListener(model.get(VideoIPHProperties.CLICK_LISTENER));
        } else if (propertyKey == VideoIPHProperties.DISMISS_LISTENER) {
            view.setDismissListener(model.get(VideoIPHProperties.DISMISS_LISTENER));
        }
    }
}
