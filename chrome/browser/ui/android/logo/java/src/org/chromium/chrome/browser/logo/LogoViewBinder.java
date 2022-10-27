// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.logo;

import android.graphics.Bitmap;
import android.view.View;
import android.view.ViewGroup.MarginLayoutParams;

import org.chromium.chrome.browser.logo.LogoBridge.Logo;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/** Responsible for building and setting properties on the logo.*/
class LogoViewBinder
        implements PropertyModelChangeProcessor.ViewBinder<PropertyModel, View, PropertyKey> {
    @Override
    public final void bind(PropertyModel model, View view, PropertyKey propertyKey) {
        LogoView logoView = (LogoView) view;

        if (LogoProperties.ALPHA == propertyKey) {
            logoView.setAlpha(model.get(LogoProperties.ALPHA));
        } else if (LogoProperties.LOGO_TOP_MARGIN == propertyKey) {
            MarginLayoutParams marginLayoutParams = (MarginLayoutParams) logoView.getLayoutParams();
            marginLayoutParams.topMargin = model.get(LogoProperties.LOGO_TOP_MARGIN);
        } else if (LogoProperties.LOGO_BOTTOM_MARGIN == propertyKey) {
            MarginLayoutParams marginLayoutParams = (MarginLayoutParams) logoView.getLayoutParams();
            marginLayoutParams.bottomMargin = model.get(LogoProperties.LOGO_BOTTOM_MARGIN);
        } else if (LogoProperties.SET_END_FADE_ANIMATION == propertyKey) {
            logoView.endFadeAnimation();
        } else if (LogoProperties.DESTROY == propertyKey) {
            logoView.destroy();
        } else if (LogoProperties.VISIBILITY == propertyKey) {
            logoView.setVisibility(model.get(LogoProperties.VISIBILITY) ? View.VISIBLE : View.GONE);
        } else if (LogoProperties.ANIMATION_ENABLED == propertyKey) {
            logoView.setAnimationEnabled(model.get(LogoProperties.ANIMATION_ENABLED));
        } else if (LogoProperties.LOGO_DELEGATE == propertyKey) {
            logoView.setDelegate((LogoDelegateImpl) model.get(LogoProperties.LOGO_DELEGATE));
        } else if (LogoProperties.SHOW_SEARCH_PROVIDER_INITIAL_VIEW == propertyKey) {
            logoView.showSearchProviderInitialView();
        } else if (LogoProperties.UPDATED_LOGO == propertyKey) {
            logoView.updateLogo((Logo) model.get(LogoProperties.UPDATED_LOGO));
        } else if (LogoProperties.DEFAULT_GOOGLE_LOGO == propertyKey) {
            logoView.setDefaultGoogleLogo((Bitmap) model.get(LogoProperties.DEFAULT_GOOGLE_LOGO));
        } else {
            assert false : "Unhandled property detected in LogoViewBinder!";
        }
    }
}
