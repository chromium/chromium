// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.logo;

import android.graphics.Bitmap;

import jp.tomorrowkey.android.gifplayer.BaseGifImage;

import org.chromium.base.Callback;
import org.chromium.chrome.browser.logo.LogoBridge.Logo;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableBooleanPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableFloatPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableIntPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;

/** The properties required to build the logo on start surface or ntp. */
interface LogoProperties {
    // TODO(crbug.com/40881870): It doesn't really make sense for those
    //  WritableObjectPropertyKey<Boolean> with skipEquality equals to true property keys;
    //  if we're not going to read the value out of this in the ViewBinder.
    WritableFloatPropertyKey ALPHA = new WritableFloatPropertyKey();
    WritableIntPropertyKey LOGO_TOP_MARGIN = new WritableIntPropertyKey();
    WritableIntPropertyKey LOGO_BOTTOM_MARGIN = new WritableIntPropertyKey();
    WritableObjectPropertyKey<Boolean> SET_END_FADE_ANIMATION =
            new WritableObjectPropertyKey<>(/* skipEquality= */ true);
    // TODO(crbug.com/40881870): Change the VISIBILITY properties to some sort of state
    //  enum if possible.
    WritableBooleanPropertyKey VISIBILITY = new WritableBooleanPropertyKey();
    WritableBooleanPropertyKey ANIMATION_ENABLED = new WritableBooleanPropertyKey();
    WritableObjectPropertyKey<LogoView.ClickHandler> LOGO_CLICK_HANDLER =
            new WritableObjectPropertyKey<>();
    WritableObjectPropertyKey<Boolean> SHOW_SEARCH_PROVIDER_INITIAL_VIEW =
            new WritableObjectPropertyKey<>(/* skipEquality= */ true);
    // TODO(crbug.com/40881870): Generate the LOGO, DEFAULT_GOOGLE_LOGO and ANIMATED_LOGO properties
    //  into one property that takes an object generic/powerful enough to represent all three of
    //  these if possible.
    WritableObjectPropertyKey<LogoBridge.Logo> LOGO = new WritableObjectPropertyKey<>();
    WritableObjectPropertyKey<Bitmap> DEFAULT_GOOGLE_LOGO = new WritableObjectPropertyKey<>();
    WritableObjectPropertyKey<Boolean> SHOW_LOADING_VIEW =
            new WritableObjectPropertyKey<>(/* skipEquality= */ true);
    WritableObjectPropertyKey<BaseGifImage> ANIMATED_LOGO = new WritableObjectPropertyKey<>();
    WritableObjectPropertyKey<Callback<Logo>> LOGO_AVAILABLE_CALLBACK =
            new WritableObjectPropertyKey<>();
    WritableObjectPropertyKey<Boolean> LOGO_POLISH_FLAG_ENABLED = new WritableObjectPropertyKey<>();
    WritableIntPropertyKey LOGO_SIZE_FOR_LOGO_POLISH = new WritableIntPropertyKey();

    PropertyKey[] ALL_KEYS =
            new PropertyKey[] {
                ALPHA,
                LOGO_TOP_MARGIN,
                LOGO_BOTTOM_MARGIN,
                SET_END_FADE_ANIMATION,
                VISIBILITY,
                ANIMATION_ENABLED,
                LOGO_CLICK_HANDLER,
                SHOW_SEARCH_PROVIDER_INITIAL_VIEW,
                LOGO,
                DEFAULT_GOOGLE_LOGO,
                SHOW_LOADING_VIEW,
                ANIMATED_LOGO,
                LOGO_AVAILABLE_CALLBACK,
                LOGO_POLISH_FLAG_ENABLED,
                LOGO_SIZE_FOR_LOGO_POLISH
            };
}
