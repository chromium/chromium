// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.video_tutorials.languages;

import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableBooleanPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;

/**
 * The properties associated with the outer layout of the language picker view.
 */
interface LanguagePickerProperties {
    /** The callback to run when the close button is clicked on the UI. */
    WritableObjectPropertyKey<Runnable> CLOSE_CALLBACK = new WritableObjectPropertyKey<>();

    /** The callback to run when the watch button is clicked on the UI. */
    WritableObjectPropertyKey<Runnable> WATCH_CALLBACK = new WritableObjectPropertyKey<>();

    /** Whether or not the watch button should be shown as enabled. */
    WritableBooleanPropertyKey IS_ENABLED_WATCH_BUTTON = new WritableBooleanPropertyKey();

    PropertyKey[] ALL_KEYS =
            new PropertyKey[] {CLOSE_CALLBACK, WATCH_CALLBACK, IS_ENABLED_WATCH_BUTTON};
}
