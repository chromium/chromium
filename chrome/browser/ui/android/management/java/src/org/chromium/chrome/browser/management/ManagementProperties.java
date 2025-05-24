// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.management;

import android.text.SpannableString;
import android.text.SpannableStringBuilder;

import org.chromium.build.annotations.NullMarked;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/** Associated properties for ManagementPage's view. */
@NullMarked
class ManagementProperties {
    public static final PropertyModel.WritableObjectPropertyKey<String> TITLE =
            new PropertyModel.WritableObjectPropertyKey<>();

    public static final PropertyModel.WritableBooleanPropertyKey BROWSER_IS_MANAGED =
            new PropertyModel.WritableBooleanPropertyKey();

    public static final PropertyModel.WritableBooleanPropertyKey PROFILE_IS_MANAGED =
            new PropertyModel.WritableBooleanPropertyKey();

    public static final PropertyModel.WritableObjectPropertyKey<SpannableString> LEARN_MORE_TEXT =
            new PropertyModel.WritableObjectPropertyKey<>();

    public static final PropertyModel.WritableBooleanPropertyKey BROWSER_REPORTING_IS_ENABLED =
            new PropertyModel.WritableBooleanPropertyKey();

    public static final PropertyModel.WritableBooleanPropertyKey PROFILE_REPORTING_IS_ENABLED =
            new PropertyModel.WritableBooleanPropertyKey();
    public static final PropertyModel.WritableObjectPropertyKey<SpannableStringBuilder>
            PROFILE_REPORTING_TEXT = new PropertyModel.WritableObjectPropertyKey<>();

    public static final PropertyModel.WritableBooleanPropertyKey LEGACY_TECH_REPORTING_IS_ENABLED =
            new PropertyModel.WritableBooleanPropertyKey();

    public static final PropertyModel.WritableObjectPropertyKey<SpannableString>
            LEGACY_TECH_REPORTING_TEXT = new PropertyModel.WritableObjectPropertyKey<>();

    public static final PropertyModel.ReadableBooleanPropertyKey SECURITY_EVENT_REPORTING_ENABLED =
            new PropertyModel.ReadableBooleanPropertyKey();

    public static final PropertyModel.ReadableObjectPropertyKey<SpannableStringBuilder>
            SECURITY_EVENT_REPORTING_TEXT = new PropertyModel.WritableObjectPropertyKey<>();

    public static final PropertyModel.ReadableObjectPropertyKey<SpannableStringBuilder>
            SECURITY_EVENT_REPORTING_DESCRIPTION_TEXT =
                    new PropertyModel.WritableObjectPropertyKey<>();

    public static final PropertyModel.ReadableBooleanPropertyKey URL_FILTERING_ENABLED =
            new PropertyModel.ReadableBooleanPropertyKey();

    public static final PropertyModel.ReadableObjectPropertyKey<SpannableStringBuilder>
            URL_FILTERING_TEXT = new PropertyModel.WritableObjectPropertyKey<>();

    public static final PropertyModel.ReadableObjectPropertyKey<SpannableStringBuilder>
            URL_FILTERING_DESCRIPTION_TEXT = new PropertyModel.WritableObjectPropertyKey<>();

    public static final PropertyKey[] ALL_KEYS = {
        TITLE,
        BROWSER_IS_MANAGED,
        PROFILE_IS_MANAGED,
        LEARN_MORE_TEXT,
        BROWSER_REPORTING_IS_ENABLED,
        PROFILE_REPORTING_IS_ENABLED,
        PROFILE_REPORTING_TEXT,
        LEGACY_TECH_REPORTING_IS_ENABLED,
        LEGACY_TECH_REPORTING_TEXT,
        SECURITY_EVENT_REPORTING_ENABLED,
        SECURITY_EVENT_REPORTING_TEXT,
        SECURITY_EVENT_REPORTING_DESCRIPTION_TEXT,
        URL_FILTERING_ENABLED,
        URL_FILTERING_TEXT,
        URL_FILTERING_DESCRIPTION_TEXT,
    };
}
