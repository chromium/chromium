// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.logo;

import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableBooleanPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableFloatPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableIntPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;

/**
 * The properties required to build the logo on start surface or ntp.
 */
interface LogoProperties {
    WritableFloatPropertyKey ALPHA = new WritableFloatPropertyKey();
    WritableIntPropertyKey LOGO_TOP_MARGIN = new WritableIntPropertyKey();
    WritableIntPropertyKey LOGO_BOTTOM_MARGIN = new WritableIntPropertyKey();
    WritableObjectPropertyKey SET_END_FADE_ANIMATION =
            new WritableObjectPropertyKey<>(true /* skipEquality */);
    WritableObjectPropertyKey DESTROY = new WritableObjectPropertyKey<>(true /* skipEquality */);
    WritableBooleanPropertyKey VISIBILITY = new WritableBooleanPropertyKey();
    WritableBooleanPropertyKey ANIMATION_ENABLED = new WritableBooleanPropertyKey();
    WritableObjectPropertyKey LOGO_DELEGATE = new WritableObjectPropertyKey<>();
    WritableObjectPropertyKey SHOW_SEARCH_PROVIDER_INITIAL_VIEW =
            new WritableObjectPropertyKey<>(true /* skipEquality */);
    WritableObjectPropertyKey UPDATED_LOGO = new WritableObjectPropertyKey<>();
    WritableObjectPropertyKey DEFAULT_GOOGLE_LOGO = new WritableObjectPropertyKey<>();

    PropertyKey[] ALL_KEYS = new PropertyKey[] {ALPHA, LOGO_TOP_MARGIN, LOGO_BOTTOM_MARGIN,
            SET_END_FADE_ANIMATION, DESTROY, VISIBILITY, ANIMATION_ENABLED, LOGO_DELEGATE,
            SHOW_SEARCH_PROVIDER_INITIAL_VIEW, UPDATED_LOGO, DEFAULT_GOOGLE_LOGO};
}
