// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.video_tutorials.player;

import org.chromium.chrome.browser.video_tutorials.PlaybackStateObserver.WatchStateInfo.State;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableBooleanPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;

/**
 * The properties required to build the video player which primarily contains (1) the screen being
 * shown on the player, e.g. loading view, language picker, or video player. (2) callbacks
 * associated with various buttons.
 */
interface VideoPlayerProperties {
    WritableBooleanPropertyKey SHOW_LOADING_SCREEN = new WritableBooleanPropertyKey();
    WritableBooleanPropertyKey SHOW_LANGUAGE_PICKER = new WritableBooleanPropertyKey();
    WritableBooleanPropertyKey SHOW_TRY_NOW = new WritableBooleanPropertyKey();
    WritableBooleanPropertyKey SHOW_SHARE = new WritableBooleanPropertyKey();
    WritableBooleanPropertyKey SHOW_CLOSE = new WritableBooleanPropertyKey();
    WritableBooleanPropertyKey SHOW_WATCH_NEXT = new WritableBooleanPropertyKey();
    WritableBooleanPropertyKey SHOW_CHANGE_LANGUAGE = new WritableBooleanPropertyKey();
    WritableBooleanPropertyKey SHOW_PLAY_BUTTON = new WritableBooleanPropertyKey();
    WritableObjectPropertyKey<Runnable> CALLBACK_PLAY_BUTTON = new WritableObjectPropertyKey();
    WritableObjectPropertyKey<String> CHANGE_LANGUAGE_BUTTON_TEXT =
            new WritableObjectPropertyKey<>();
    WritableObjectPropertyKey<Runnable> CALLBACK_WATCH_NEXT = new WritableObjectPropertyKey<>();
    WritableObjectPropertyKey<Runnable> CALLBACK_CHANGE_LANGUAGE =
            new WritableObjectPropertyKey<>();
    WritableObjectPropertyKey<Runnable> CALLBACK_TRY_NOW = new WritableObjectPropertyKey<>();
    WritableObjectPropertyKey<Runnable> CALLBACK_SHARE = new WritableObjectPropertyKey<>();
    WritableObjectPropertyKey<Runnable> CALLBACK_CLOSE = new WritableObjectPropertyKey<>();
    WritableObjectPropertyKey<State> WATCH_STATE_FOR_TRY_NOW = new WritableObjectPropertyKey<>();

    PropertyKey[] ALL_KEYS = new PropertyKey[] {SHOW_LOADING_SCREEN, SHOW_LANGUAGE_PICKER,
            SHOW_TRY_NOW, SHOW_SHARE, SHOW_CLOSE, SHOW_WATCH_NEXT, SHOW_CHANGE_LANGUAGE,
            SHOW_PLAY_BUTTON, CALLBACK_PLAY_BUTTON, CHANGE_LANGUAGE_BUTTON_TEXT,
            CALLBACK_WATCH_NEXT, CALLBACK_CHANGE_LANGUAGE, CALLBACK_TRY_NOW, CALLBACK_SHARE,
            CALLBACK_CLOSE, WATCH_STATE_FOR_TRY_NOW};
}
