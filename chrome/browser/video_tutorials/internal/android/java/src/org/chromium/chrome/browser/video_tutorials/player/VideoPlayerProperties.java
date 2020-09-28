// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.video_tutorials.player;

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
    WritableBooleanPropertyKey SHOW_MEDIA_CONTROLS = new WritableBooleanPropertyKey();
    WritableBooleanPropertyKey SHOW_LANGUAGE_PICKER = new WritableBooleanPropertyKey();
    WritableBooleanPropertyKey SHOW_WATCH_NEXT = new WritableBooleanPropertyKey();
    WritableBooleanPropertyKey SHOW_CHANGE_LANGUAGE = new WritableBooleanPropertyKey();
    WritableObjectPropertyKey<String> CHANGE_LANGUAGE_BUTTON_TEXT =
            new WritableObjectPropertyKey<>();
    WritableObjectPropertyKey<Runnable> CALLBACK_WATCH_NEXT = new WritableObjectPropertyKey<>();
    WritableObjectPropertyKey<Runnable> CALLBACK_CHANGE_LANGUAGE =
            new WritableObjectPropertyKey<>();
    WritableObjectPropertyKey<Runnable> CALLBACK_TRY_NOW = new WritableObjectPropertyKey<>();
    WritableObjectPropertyKey<Runnable> CALLBACK_SHARE = new WritableObjectPropertyKey<>();
    WritableObjectPropertyKey<Runnable> CALLBACK_CLOSE = new WritableObjectPropertyKey<>();

    PropertyKey[] ALL_KEYS = new PropertyKey[] {SHOW_LOADING_SCREEN, SHOW_MEDIA_CONTROLS,
            SHOW_LANGUAGE_PICKER, SHOW_WATCH_NEXT, SHOW_CHANGE_LANGUAGE,
            CHANGE_LANGUAGE_BUTTON_TEXT, CALLBACK_WATCH_NEXT, CALLBACK_CHANGE_LANGUAGE,
            CALLBACK_TRY_NOW, CALLBACK_SHARE, CALLBACK_CLOSE};
}
