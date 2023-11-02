// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.video_tutorials.iph;

import android.view.View;
import android.view.ViewStub;
import android.widget.TextView;

import org.chromium.chrome.browser.video_tutorials.R;
import org.chromium.components.browser_ui.widget.async_image.AsyncImageView;
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

    private void showVideoLength(boolean show) {
        TextView view = mCardView.findViewById(R.id.video_length);
        view.setVisibility(show ? View.VISIBLE : View.GONE);
    }

    private View getThumbnailView() {
        return mCardView.findViewById(R.id.thumbnail);
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
        } else if (propertyKey == VideoIPHProperties.SHOW_VIDEO_LENGTH) {
            view.showVideoLength(model.get(VideoIPHProperties.SHOW_VIDEO_LENGTH));
        } else if (propertyKey == VideoIPHProperties.CLICK_LISTENER) {
            view.setClickListener(model.get(VideoIPHProperties.CLICK_LISTENER));
        } else if (propertyKey == VideoIPHProperties.DISMISS_LISTENER) {
            view.setDismissListener(model.get(VideoIPHProperties.DISMISS_LISTENER));
        } else if (propertyKey == VideoIPHProperties.THUMBNAIL_PROVIDER) {
            AsyncImageView thumbnailView = (AsyncImageView) view.getThumbnailView();
            thumbnailView.setAsyncImageDrawable(
                    model.get(VideoIPHProperties.THUMBNAIL_PROVIDER), null);
        }
    }
}
