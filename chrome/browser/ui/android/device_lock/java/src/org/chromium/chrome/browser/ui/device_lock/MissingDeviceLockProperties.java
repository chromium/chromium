// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.device_lock;

import android.view.View.OnClickListener;
import android.widget.CompoundButton;

import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel.ReadableObjectPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableBooleanPropertyKey;

public class MissingDeviceLockProperties {
    static final WritableBooleanPropertyKey REMOVE_ALL_LOCAL_DATA_CHECKED =
            new WritableBooleanPropertyKey();
    static final ReadableObjectPropertyKey<OnClickListener> ON_CONTINUE_CLICKED =
            new ReadableObjectPropertyKey<>();
    static final ReadableObjectPropertyKey<OnClickListener> ON_CREATE_DEVICE_LOCK_CLICKED =
            new ReadableObjectPropertyKey<>();
    static final ReadableObjectPropertyKey<CompoundButton.OnCheckedChangeListener>
            ON_CHECKBOX_TOGGLED = new ReadableObjectPropertyKey<>();

    static final PropertyKey[] ALL_KEYS =
            new PropertyKey[] {
                REMOVE_ALL_LOCAL_DATA_CHECKED,
                ON_CONTINUE_CLICKED,
                ON_CREATE_DEVICE_LOCK_CLICKED,
                ON_CHECKBOX_TOGGLED,
            };

    private MissingDeviceLockProperties() {}
}
