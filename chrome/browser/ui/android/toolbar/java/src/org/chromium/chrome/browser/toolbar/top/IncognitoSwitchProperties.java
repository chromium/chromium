// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.top;

import android.widget.CompoundButton.OnCheckedChangeListener;

import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/** List of the incognito switch properties. */
class IncognitoSwitchProperties {
    public static final PropertyModel.WritableBooleanPropertyKey IS_VISIBLE =
            new PropertyModel.WritableBooleanPropertyKey();
    public static final PropertyModel.WritableBooleanPropertyKey IS_INCOGNITO =
            new PropertyModel.WritableBooleanPropertyKey();
    public static final PropertyModel
            .WritableObjectPropertyKey<OnCheckedChangeListener> ON_CHECKED_CHANGE_LISTENER =
            new PropertyModel.WritableObjectPropertyKey<OnCheckedChangeListener>();
    public static final PropertyKey[] ALL_KEYS =
            new PropertyKey[] {IS_VISIBLE, IS_INCOGNITO, ON_CHECKED_CHANGE_LISTENER};
}
