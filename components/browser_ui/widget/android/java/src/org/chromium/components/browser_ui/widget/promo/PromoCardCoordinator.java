// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.widget.promo;

import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;

import androidx.annotation.IntDef;
import androidx.annotation.LayoutRes;
import androidx.annotation.Nullable;

import org.chromium.components.browser_ui.widget.R;
import org.chromium.components.browser_ui.widget.impression.ImpressionTracker;
import org.chromium.components.browser_ui.widget.impression.OneShotImpressionListener;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * PromoCard Coordinator that owns the view and the model change processor. Client will need to
 * create another layer of controller to own this coordinator, and pass in the {@link PropertyModel}
 * to initialize the view.
 */
public class PromoCardCoordinator {
    @IntDef({LayoutStyle.LARGE, LayoutStyle.COMPACT, LayoutStyle.SLIM})
    @Retention(RetentionPolicy.SOURCE)
    public @interface LayoutStyle {
        int LARGE = 0;
        int COMPACT = 1;
        int SLIM = 2;
    }

    private static final double IMPRESSION_THRESHOLD_RATIO = 0.75;

    private PromoCardView mPromoCardView;
    private PropertyModelChangeProcessor mModelChangeProcessor;
    private String mFeatureName;

    private @Nullable ImpressionTracker mImpressionTracker;

    /**
     * Create the Coordinator of PromoCard that owns the view and the change process. Default to
     * create the large variance.
     *
     * @param context Context used to create the view.
     * @param model {@link PropertyModel} built with {@link PromoCardProperties}.
     * @param featureName Name of the feature of this promo. Will be used to create keys for
     *     SharedPreference.
     */
    public static PromoCardCoordinator create(
            Context context, PropertyModel model, String featureName) {
        return PromoCardCoordinator.create(context, model, featureName, LayoutStyle.COMPACT);
    }

    /**
     * Create the view and the Coordinator of PromoCard that owns the view and the change process.
     *
     * @param context Context used to create the view.
     * @param model {@link PropertyModel} built with {@link PromoCardProperties}.
     * @param featureName Name of the feature of this promo. Will be used to create keys for
     *     SharedPreference.
     * @param layoutStyle {@link LayoutStyle} used for the promo.
     */
    public static PromoCardCoordinator create(
            Context context,
            PropertyModel model,
            String featureName,
            @LayoutStyle int layoutStyle) {
        PromoCardView promoCardView =
                (PromoCardView)
                        LayoutInflater.from(context)
                                .inflate(getPromoLayout(layoutStyle), null, false);
        return new PromoCardCoordinator(promoCardView, model, featureName);
    }

    /**
     * Create the Coordinator of PromoCard that owns the view and the change process.
     *
     * @param context Context used to create the view.
     * @param model {@link PropertyModel} built with {@link PromoCardProperties}.
     * @param featureName Name of the feature of this promo. Will be used to create keys for
     *     SharedPreference.
     * @param layoutStyle {@link LayoutStyle} used for the promo.
     */
    public static PromoCardCoordinator createFromView(
            View view, PropertyModel model, String featureName) {
        return new PromoCardCoordinator((PromoCardView) view, model, featureName);
    }

    /**
     * Create the Coordinator of PromoCard that owns the view and the change process.
     *
     * @param view the {@link PromoCardView}
     * @param model {@link PropertyModel} built with {@link PromoCardProperties}.
     * @param featureName Name of the feature of this promo. Will be used to create keys for
     *     SharedPreference.
     */
    public PromoCardCoordinator(PromoCardView view, PropertyModel model, String featureName) {
        mPromoCardView = view;

        mModelChangeProcessor =
                PropertyModelChangeProcessor.create(
                        model, mPromoCardView, new PromoCardViewBinder());
        mFeatureName = featureName;

        // Manage impression related properties.
        Runnable impressionCallback = model.get(PromoCardProperties.IMPRESSION_SEEN_CALLBACK);
        if (impressionCallback != null) {
            boolean isImpressionOnPrimaryButton =
                    model.get(PromoCardProperties.IS_IMPRESSION_ON_PRIMARY_BUTTON);
            mImpressionTracker =
                    new ImpressionTracker(
                            isImpressionOnPrimaryButton
                                    ? mPromoCardView.mPrimaryButton
                                    : mPromoCardView);
            // TODO(wenyufu): Maybe make the ratio configurable?
            mImpressionTracker.setImpressionThresholdRatio(IMPRESSION_THRESHOLD_RATIO);
            mImpressionTracker.setListener(new OneShotImpressionListener(impressionCallback::run));
        }
    }

    /** Destroy the PromoCard component and release dependencies. */
    public void destroy() {
        mModelChangeProcessor.destroy();
        if (mImpressionTracker != null) mImpressionTracker.setListener(null);
        mImpressionTracker = null;
    }

    /** @return {@link PromoCardView} held by this promo component. */
    public View getView() {
        return mPromoCardView;
    }

    /** @return Name of the feature this promo is representing. */
    public String getFeatureName() {
        return mFeatureName;
    }

    private static @LayoutRes int getPromoLayout(@LayoutStyle int layoutStyle) {
        switch (layoutStyle) {
            case LayoutStyle.LARGE:
                return R.layout.promo_card_view_large;
            case LayoutStyle.COMPACT:
                return R.layout.promo_card_view_compact;
            case LayoutStyle.SLIM:
                return R.layout.promo_card_view_slim;
            default:
                throw new IllegalArgumentException("Unsupported value: " + layoutStyle);
        }
    }
}
