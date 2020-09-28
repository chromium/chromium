// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.video_tutorials.player;

import android.view.View;
import android.widget.TextView;

import org.chromium.chrome.browser.video_tutorials.R;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor.ViewBinder;

/**
 * The binder to bind the video player property model with the {@link VideoPlayerView}.
 */
class VideoPlayerViewBinder implements ViewBinder<PropertyModel, VideoPlayerView, PropertyKey> {
    @Override
    public void bind(PropertyModel model, VideoPlayerView view, PropertyKey propertyKey) {
        if (propertyKey == VideoPlayerProperties.SHOW_LOADING_SCREEN) {
            view.showLoadingAnimation(model.get(VideoPlayerProperties.SHOW_LOADING_SCREEN));
        } else if (propertyKey == VideoPlayerProperties.SHOW_MEDIA_CONTROLS) {
            view.showMediaControls(model.get(VideoPlayerProperties.SHOW_MEDIA_CONTROLS));
        } else if (propertyKey == VideoPlayerProperties.SHOW_LANGUAGE_PICKER) {
            view.showLanguagePicker(model.get(VideoPlayerProperties.SHOW_LANGUAGE_PICKER));
        } else if (propertyKey == VideoPlayerProperties.SHOW_WATCH_NEXT) {
            view.getView()
                    .findViewById(R.id.watch_next)
                    .setVisibility(model.get(VideoPlayerProperties.SHOW_WATCH_NEXT) ? View.VISIBLE
                                                                                    : View.GONE);
        } else if (propertyKey == VideoPlayerProperties.SHOW_CHANGE_LANGUAGE) {
            view.getView()
                    .findViewById(R.id.change_language)
                    .setVisibility(model.get(VideoPlayerProperties.SHOW_CHANGE_LANGUAGE)
                                    ? View.VISIBLE
                                    : View.GONE);
        } else if (propertyKey == VideoPlayerProperties.CHANGE_LANGUAGE_BUTTON_TEXT) {
            TextView textView = view.getView().findViewById(R.id.change_language);
            textView.setText(model.get(VideoPlayerProperties.CHANGE_LANGUAGE_BUTTON_TEXT));
        } else if (propertyKey == VideoPlayerProperties.CALLBACK_CLOSE) {
            view.getView().findViewById(R.id.close_button).setOnClickListener(v -> {
                model.get(VideoPlayerProperties.CALLBACK_CLOSE).run();
            });
        } else if (propertyKey == VideoPlayerProperties.CALLBACK_SHARE) {
            view.getView().findViewById(R.id.share_button).setOnClickListener(v -> {
                model.get(VideoPlayerProperties.CALLBACK_SHARE).run();
            });
        } else if (propertyKey == VideoPlayerProperties.CALLBACK_WATCH_NEXT) {
            view.getView().findViewById(R.id.watch_next).setOnClickListener(v -> {
                model.get(VideoPlayerProperties.CALLBACK_WATCH_NEXT).run();
            });
        } else if (propertyKey == VideoPlayerProperties.CALLBACK_TRY_NOW) {
            view.getView().findViewById(R.id.try_now).setOnClickListener(v -> {
                model.get(VideoPlayerProperties.CALLBACK_TRY_NOW).run();
            });
        } else if (propertyKey == VideoPlayerProperties.CALLBACK_CHANGE_LANGUAGE) {
            view.getView().findViewById(R.id.change_language).setOnClickListener(v -> {
                model.get(VideoPlayerProperties.CALLBACK_CHANGE_LANGUAGE).run();
            });
        }
    }
}
