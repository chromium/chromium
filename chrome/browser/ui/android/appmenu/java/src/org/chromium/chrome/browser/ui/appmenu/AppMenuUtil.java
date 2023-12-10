// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.appmenu;

import android.animation.Animator;
import android.animation.AnimatorListenerAdapter;
import android.animation.AnimatorSet;
import android.animation.ObjectAnimator;
import android.view.MenuItem;
import android.view.View;
import android.widget.ImageView;

import org.chromium.ui.base.LocalizationUtils;
import org.chromium.ui.interpolators.Interpolators;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/** This is a helper class for app menu. */
public class AppMenuUtil {
    /** MenuItem Animation Constants */
    static final int ENTER_ITEM_DURATION_MS = 350;

    static final int ENTER_ITEM_BASE_DELAY_MS = 80;
    static final int ENTER_ITEM_ADDL_DELAY_MS = 30;
    static final float ENTER_STANDARD_ITEM_OFFSET_Y_DP = -10.f;
    static final float ENTER_STANDARD_ITEM_OFFSET_X_DP = 10.f;

    /**
     * This builds an {@link Animator} for the enter animation of a standard menu item.  This means
     * it will animate the alpha from 0 to 1 and translate the view from -10dp to 0dp on the y axis.
     *
     * @param view     The menu item {@link View} to be animated.
     * @param position The position in the menu.  This impacts the start delay of the animation.
     * @return         The {@link Animator}.
     */
    public static Animator buildStandardItemEnterAnimator(final View view, int position) {
        float dpToPx = view.getContext().getResources().getDisplayMetrics().density;
        final int startDelay = ENTER_ITEM_BASE_DELAY_MS + ENTER_ITEM_ADDL_DELAY_MS * position;

        AnimatorSet animation = new AnimatorSet();
        final float offsetYPx = ENTER_STANDARD_ITEM_OFFSET_Y_DP * dpToPx;
        animation.playTogether(
                ObjectAnimator.ofFloat(view, View.ALPHA, 0.f, 1.f),
                ObjectAnimator.ofFloat(view, View.TRANSLATION_Y, offsetYPx, 0.f));
        animation.setStartDelay(startDelay);
        animation.setDuration(ENTER_ITEM_DURATION_MS);
        animation.setInterpolator(Interpolators.EMPHASIZED);

        animation.addListener(
                new AnimatorListenerAdapter() {
                    @Override
                    public void onAnimationStart(Animator animation) {
                        view.setAlpha(0.f);
                    }
                });
        return animation;
    }

    /**
     * This builds an {@link Animator} for the enter animation of icon row menu items.  This means
     * it will animate the alpha from 0 to 1 and translate the views from 10dp to 0dp on the x axis.
     *
     * @param buttons The list of icons in the menu item that should be animated.
     * @param isMenuIconAtStart Whether the menu was triggered from a menu icon positioned at start.
     * @return        The {@link Animator}.
     */
    public static Animator buildIconItemEnterAnimator(
            final ImageView[] buttons, boolean isMenuIconAtStart) {
        if (buttons.length < 1) return new AnimatorSet();
        float dpToPx = buttons[0].getContext().getResources().getDisplayMetrics().density;
        final boolean rtl = LocalizationUtils.isLayoutRtl();
        float offsetSign = (rtl == /* XNOR */ isMenuIconAtStart) ? 1.f : -1.f;
        final float offsetXPx = ENTER_STANDARD_ITEM_OFFSET_X_DP * dpToPx * offsetSign;
        final int maxViewsToAnimate = buttons.length;

        AnimatorSet animation = new AnimatorSet();
        AnimatorSet.Builder builder = null;
        for (int i = 0; i < maxViewsToAnimate; i++) {
            final int startDelay = ENTER_ITEM_ADDL_DELAY_MS * i;

            ImageView view = buttons[i];
            Animator alpha = ObjectAnimator.ofFloat(view, View.ALPHA, 0.f, 1.f);
            Animator translate = ObjectAnimator.ofFloat(view, View.TRANSLATION_X, offsetXPx, 0);
            alpha.setStartDelay(startDelay);
            translate.setStartDelay(startDelay);
            alpha.setDuration(ENTER_ITEM_DURATION_MS);
            translate.setDuration(ENTER_ITEM_DURATION_MS);

            if (builder == null) {
                builder = animation.play(alpha);
            } else {
                builder.with(alpha);
            }
            builder.with(translate);
        }
        animation.setStartDelay(ENTER_ITEM_BASE_DELAY_MS);
        animation.setInterpolator(Interpolators.LINEAR_OUT_SLOW_IN_INTERPOLATOR);

        animation.addListener(
                new AnimatorListenerAdapter() {
                    @Override
                    public void onAnimationStart(Animator animation) {
                        for (int i = 0; i < maxViewsToAnimate; i++) {
                            buttons[i].setAlpha(0.f);
                        }
                    }
                });
        return animation;
    }

    /**
     * Create a {@link PropertyModel} from a {@link MenuItem}.
     *
     * @param menuItem The MenuItem which need to be transferred to the {@link PropertyModel}.
     * @return         The {@link PropertyModel}.
     */
    public static PropertyModel menuItemToPropertyModel(MenuItem menuItem) {
        return new PropertyModel.Builder(AppMenuItemProperties.ALL_KEYS)
                .with(AppMenuItemProperties.MENU_ITEM_ID, menuItem.getItemId())
                .with(AppMenuItemProperties.TITLE, menuItem.getTitle())
                .with(AppMenuItemProperties.TITLE_CONDENSED, menuItem.getTitleCondensed())
                .with(AppMenuItemProperties.ICON, menuItem.getIcon())
                .with(AppMenuItemProperties.CHECKABLE, menuItem.isCheckable())
                .with(AppMenuItemProperties.CHECKED, menuItem.isChecked())
                .with(AppMenuItemProperties.ENABLED, menuItem.isEnabled())
                .build();
    }

    /**
     * builds a enter animation of a standard menu item.
     *
     * @param model The model containing the data for the view.
     * @param view The view to be animated.
     * @param key The key of the property to be bound.
     */
    public static void bindStandardItemEnterAnimation(
            PropertyModel model, View view, PropertyKey key) {
        if (key == AppMenuItemProperties.SUPPORT_ENTER_ANIMATION) {
            if (model.get(AppMenuItemProperties.SUPPORT_ENTER_ANIMATION)) {
                int position = model.get(AppMenuItemProperties.POSITION);
                view.setTag(
                        R.id.menu_item_enter_anim_id,
                        buildStandardItemEnterAnimator(view, position));
            } else {
                view.setTag(R.id.menu_item_enter_anim_id, null);
            }
        }
    }
}
