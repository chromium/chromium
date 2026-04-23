// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.logo;

import android.view.View;

import org.chromium.build.annotations.NullMarked;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/** Responsible for building and setting properties on the logo. */
@NullMarked
class LogoContainerViewBinder
        implements PropertyModelChangeProcessor.ViewBinder<PropertyModel, View, PropertyKey> {
    @Override
    public final void bind(PropertyModel model, View view, PropertyKey propertyKey) {
        LogoContainerView logoContainerView = (LogoContainerView) view;

        if (LogoProperties.ALPHA == propertyKey) {
            logoContainerView.setAlpha(model.get(LogoProperties.ALPHA));
        } else if (LogoProperties.LOGO_TOP_MARGIN == propertyKey) {
            logoContainerView.setLogoTopMargin(model.get(LogoProperties.LOGO_TOP_MARGIN));
        } else if (LogoProperties.LOGO_BOTTOM_MARGIN == propertyKey) {
            logoContainerView.setLogoBottomMargin(model.get(LogoProperties.LOGO_BOTTOM_MARGIN));
        } else if (LogoProperties.LOGO_HEIGHT == propertyKey) {
            logoContainerView.setLogoHeight(model.get(LogoProperties.LOGO_HEIGHT));
        } else if (LogoProperties.SET_END_FADE_ANIMATION == propertyKey) {
            logoContainerView.endFadeAnimation();
        } else if (LogoProperties.VISIBILITY == propertyKey) {
            logoContainerView.setVisibility(
                    model.get(LogoProperties.VISIBILITY) ? View.VISIBLE : View.GONE);
        } else if (LogoProperties.ANIMATION_ENABLED == propertyKey) {
            logoContainerView.setAnimationEnabled(model.get(LogoProperties.ANIMATION_ENABLED));
        } else if (LogoProperties.LOGO_CLICK_HANDLER == propertyKey) {
            logoContainerView.setClickHandler(model.get(LogoProperties.LOGO_CLICK_HANDLER));
        } else if (LogoProperties.SHOW_SEARCH_PROVIDER_INITIAL_VIEW == propertyKey) {
            logoContainerView.showSearchProviderInitialView();
        } else if (LogoProperties.LOGO == propertyKey) {
            logoContainerView.updateLogo(model.get(LogoProperties.LOGO));
        } else if (LogoProperties.DEFAULT_GOOGLE_LOGO_DRAWABLE == propertyKey) {
            logoContainerView.setDefaultGoogleLogoDrawable(
                    model.get(LogoProperties.DEFAULT_GOOGLE_LOGO_DRAWABLE));
        } else if (LogoProperties.SHOW_LOADING_VIEW == propertyKey) {
            logoContainerView.showLoadingView();
        } else if (LogoProperties.ANIMATED_LOGO == propertyKey) {
            logoContainerView.playAnimatedLogo(model.get(LogoProperties.ANIMATED_LOGO));
        } else if (LogoProperties.LOGO_AVAILABLE_CALLBACK == propertyKey) {
            logoContainerView.setLogoAvailableCallback(
                    model.get(LogoProperties.LOGO_AVAILABLE_CALLBACK));
        } else if (LogoProperties.DOODLE_SIZE == propertyKey) {
            logoContainerView.setDoodleSize(model.get(LogoProperties.DOODLE_SIZE));
        } else if (LogoProperties.SHOW_DEFAULT_GOOGLE_LOGO == propertyKey) {
            logoContainerView.maybeShowDefaultLogoDrawable();
        } else {
            assert false : "Unhandled property detected in LogoContainerViewBinder!";
        }
    }
}
