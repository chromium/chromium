// Copyright 2020 The Chromium Authors
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
        } else if (propertyKey == VideoPlayerProperties.SHOW_LANGUAGE_PICKER) {
            view.showLanguagePicker(model.get(VideoPlayerProperties.SHOW_LANGUAGE_PICKER));
        } else if (propertyKey == VideoPlayerProperties.SHOW_TRY_NOW) {
            view.getView()
                    .findViewById(R.id.try_now)
                    .setVisibility(model.get(VideoPlayerProperties.SHOW_TRY_NOW) ? View.VISIBLE
                                                                                 : View.GONE);
        } else if (propertyKey == VideoPlayerProperties.SHOW_SHARE) {
            view.getView()
                    .findViewById(R.id.share_button)
                    .setVisibility(
                            model.get(VideoPlayerProperties.SHOW_SHARE) ? View.VISIBLE : View.GONE);
        } else if (propertyKey == VideoPlayerProperties.SHOW_CLOSE) {
            view.getView()
                    .findViewById(R.id.close_button)
                    .setVisibility(
                            model.get(VideoPlayerProperties.SHOW_CLOSE) ? View.VISIBLE : View.GONE);
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
        } else if (propertyKey == VideoPlayerProperties.SHOW_PLAY_BUTTON) {
            view.getView()
                    .findViewById(R.id.play_button)
                    .setVisibility(model.get(VideoPlayerProperties.SHOW_PLAY_BUTTON) ? View.VISIBLE
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
        } else if (propertyKey == VideoPlayerProperties.CALLBACK_PLAY_BUTTON) {
            view.getView().findViewById(R.id.play_button).setOnClickListener(v -> {
                model.get(VideoPlayerProperties.CALLBACK_PLAY_BUTTON).run();
            });
        } else if (propertyKey == VideoPlayerProperties.WATCH_STATE_FOR_TRY_NOW) {
            view.setTryNowButtonPosition(model.get(VideoPlayerProperties.WATCH_STATE_FOR_TRY_NOW));
        }
    }
}
