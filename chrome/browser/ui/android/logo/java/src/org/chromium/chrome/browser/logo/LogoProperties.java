// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.logo;

import android.graphics.drawable.Drawable;

import org.chromium.base.Callback;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.logo.LogoBridge.Logo;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableBooleanPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableFloatPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableIntPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;

/** The properties required to build the logo on start surface or ntp. */
@NullMarked
interface LogoProperties {
    // TODO(crbug.com/492453183): Move interface ClickHandler to LogoView when cleanup the feature
    // flag LogoViewRefactor.
    /** Handles tasks for the {@link LogoView} shown on an NTP. */
    interface ClickHandler {
        /**
         * Called when the user clicks on the logo.
         *
         * @param isAnimatedLogoShowing Whether the animated GIF logo is playing.
         */
        void onLogoClicked(boolean isAnimatedLogoShowing);
    }

    // TODO(crbug.com/40881870): It doesn't really make sense for those
    //  WritableObjectPropertyKey<Boolean> with skipEquality equals to true property keys;
    //  if we're not going to read the value out of this in the ViewBinder.
    WritableFloatPropertyKey ALPHA = new WritableFloatPropertyKey();
    WritableIntPropertyKey LOGO_TOP_MARGIN = new WritableIntPropertyKey();
    WritableIntPropertyKey LOGO_BOTTOM_MARGIN = new WritableIntPropertyKey();
    WritableIntPropertyKey LOGO_HEIGHT = new WritableIntPropertyKey();
    WritableObjectPropertyKey<Boolean> SET_END_FADE_ANIMATION =
            new WritableObjectPropertyKey<>(/* skipEquality= */ true);
    // TODO(crbug.com/40881870): Change the VISIBILITY properties to some sort of state
    //  enum if possible.
    WritableBooleanPropertyKey VISIBILITY = new WritableBooleanPropertyKey();
    WritableBooleanPropertyKey ANIMATION_ENABLED = new WritableBooleanPropertyKey();
    WritableObjectPropertyKey<ClickHandler> LOGO_CLICK_HANDLER = new WritableObjectPropertyKey<>();
    WritableObjectPropertyKey<Boolean> SHOW_SEARCH_PROVIDER_INITIAL_VIEW =
            new WritableObjectPropertyKey<>(/* skipEquality= */ true);
    // TODO(crbug.com/40881870): Generate the LOGO, DEFAULT_GOOGLE_LOGO and ANIMATED_LOGO properties
    //  into one property that takes an object generic/powerful enough to represent all three of
    //  these if possible.
    WritableObjectPropertyKey<LogoBridge.Logo> LOGO = new WritableObjectPropertyKey<>();
    WritableObjectPropertyKey<Drawable> DEFAULT_GOOGLE_LOGO_DRAWABLE =
            new WritableObjectPropertyKey<>();
    WritableObjectPropertyKey<Boolean> SHOW_LOADING_VIEW =
            new WritableObjectPropertyKey<>(/* skipEquality= */ true);
    // TODO(crbug.com/434200490): Replace Object reference with AnimatedImageDrawable when the
    // refactoring is fully rolled out.
    WritableObjectPropertyKey<Object> ANIMATED_LOGO = new WritableObjectPropertyKey<>();
    WritableObjectPropertyKey<Callback<Logo>> LOGO_AVAILABLE_CALLBACK =
            new WritableObjectPropertyKey<>();
    WritableIntPropertyKey DOODLE_SIZE = new WritableIntPropertyKey();
    WritableObjectPropertyKey<Boolean> SHOW_DEFAULT_GOOGLE_LOGO =
            new WritableObjectPropertyKey<>(/* skipEquality= */ true);

    PropertyKey[] ALL_KEYS =
            new PropertyKey[] {
                ALPHA,
                LOGO_TOP_MARGIN,
                LOGO_BOTTOM_MARGIN,
                LOGO_HEIGHT,
                SET_END_FADE_ANIMATION,
                VISIBILITY,
                ANIMATION_ENABLED,
                LOGO_CLICK_HANDLER,
                SHOW_SEARCH_PROVIDER_INITIAL_VIEW,
                LOGO,
                DEFAULT_GOOGLE_LOGO_DRAWABLE,
                SHOW_LOADING_VIEW,
                ANIMATED_LOGO,
                LOGO_AVAILABLE_CALLBACK,
                DOODLE_SIZE,
                SHOW_DEFAULT_GOOGLE_LOGO
            };
}
