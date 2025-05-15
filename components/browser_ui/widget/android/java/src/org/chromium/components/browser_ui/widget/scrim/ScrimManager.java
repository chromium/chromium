// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.widget.scrim;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.content.Context;
import android.graphics.Color;
import android.view.ViewGroup;

import androidx.annotation.ColorInt;
import androidx.core.graphics.ColorUtils;
import androidx.core.util.Function;

import org.chromium.base.Callback;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.ArrayList;
import java.util.Collections;
import java.util.HashMap;
import java.util.Iterator;
import java.util.List;
import java.util.Map;

/**
 * Public interface to create/display/observer multiple simultaneous scrims. Clients should use
 * {@link #showScrim(PropertyModel)} to create and show a scrim. They should retain a reference to
 * their model as it will be needed to subsequently interact with this scrim, especially for hiding.
 *
 * <p>Note that allowing default scrim animations and controlling alpha manually via {@link
 * #setAlpha(float, PropertyModel)} are mutually exclusive.
 */
@NullMarked
public class ScrimManager {
    private final ObservableSupplierImpl<Boolean> mScrimVisibilitySupplier =
            new ObservableSupplierImpl<>(false);
    private final ObservableSupplierImpl<Integer> mStatusBarColorSupplier =
            new ObservableSupplierImpl<>(ScrimProperties.INVALID_COLOR);
    private final ObservableSupplierImpl<Integer> mNavigationBarColorSupplier =
            new ObservableSupplierImpl<>(ScrimProperties.INVALID_COLOR);

    private final Context mContext;
    private final ViewGroup mParent;
    private final Map<PropertyModel, ScrimCoordinator> mModelToScrim = new HashMap<>();
    private final ScrimCoordinator.Observer mOnScrimVisibilityChanged = this::pruneHiddenScrims;
    private final Callback<Integer> mOnStatusBarColorChanged = this::updateStatusBarColor;
    private final Callback<Integer> mOnNavBarColorChanged = this::updateNavBarColor;

    private boolean mDisableAnimationForTesting;

    /**
     * @param context An Android {@link Context} for creating the view.
     * @param parent The {@link ViewGroup} the scrim should exist in.
     */
    public ScrimManager(Context context, ViewGroup parent) {
        mContext = context;
        mParent = parent;
    }

    /** Performs tear down. Removes outstanding scrims and resets suppliers. */
    public void destroy() {
        for (ScrimCoordinator coordinator : mModelToScrim.values()) {
            destroyScrim(coordinator);
        }
        mModelToScrim.clear();
        mScrimVisibilitySupplier.set(false);
        mStatusBarColorSupplier.set(ScrimProperties.INVALID_COLOR);
        mNavigationBarColorSupplier.set(ScrimProperties.INVALID_COLOR);
    }

    /** Temporary alternative to {@link #getScrimVisibilitySupplier()} to make migration easier. */
    @Deprecated
    public boolean isShowingScrim() {
        return assumeNonNull(mScrimVisibilitySupplier.get());
    }

    /** Temporary alternative to {@link #getScrimVisibilitySupplier()} to make migration easier. */
    @Deprecated
    public void addObserver(ScrimCoordinator.Observer observer) {
        mScrimVisibilitySupplier.addObserver(observer);
    }

    /** Temporary alternative to {@link #getScrimVisibilitySupplier()} to make migration easier. */
    @Deprecated
    public void removeObserver(ScrimCoordinator.Observer observer) {
        mScrimVisibilitySupplier.removeObserver(observer);
    }

    /** Returns observable visibility information about all scrims. */
    public ObservableSupplier<Boolean> getScrimVisibilitySupplier() {
        return mScrimVisibilitySupplier;
    }

    /**
     * Returns observable composite color information that's the result of all scrims effecting the
     * status bar.
     */
    public ObservableSupplier<Integer> getStatusBarColorSupplier() {
        return mStatusBarColorSupplier;
    }

    /**
     * Returns observable composite color information that's the result of all scrims effecting the
     * navigation bar.
     */
    public ObservableSupplier<Integer> getNavigationBarColorSupplier() {
        return mNavigationBarColorSupplier;
    }

    /**
     * Shows a new scrim.
     *
     * @param model Contains information about the scrim to show. Callers should retain a reference
     *     to this to subsequently interact with the resulting scrim.
     */
    public void showScrim(PropertyModel model) {
        showScrim(model, true);
    }

    /**
     * Shows a new scrim.
     *
     * @param model Contains information about the scrim to show. Callers should retain a reference
     *     to this to subsequently interact with the resulting scrim.
     * @param animate Whether the scrim should animate.
     */
    public void showScrim(PropertyModel model, boolean animate) {
        ScrimCoordinator coordinator = new ScrimCoordinator(mContext, mParent);
        mModelToScrim.put(model, coordinator);
        mScrimVisibilitySupplier.set(true);
        coordinator.showScrim(model, animate);

        coordinator.addObserver(mOnScrimVisibilityChanged);
        coordinator.getStatusBarColorSupplier().addObserver(mOnStatusBarColorChanged);
        coordinator.getNavigationBarColorSupplier().addObserver(mOnNavBarColorChanged);

        if (mDisableAnimationForTesting) {
            coordinator.disableAnimationForTesting(mDisableAnimationForTesting);
        }
    }

    /**
     * Hides a given scrim.
     *
     * @param model The model used to show/identify the current scrim. If this model does not
     *     correspond to the current scrim, then this request will be ignored.
     * @param animate Whether the scrim should animate and fade out.
     */
    public void hideScrim(PropertyModel model, boolean animate) {
        hideScrim(model, animate, ScrimCoordinator.ANIM_DURATION_MS);
    }

    /**
     * Hides a given scrim.
     *
     * @param model The model used to show/identify the current scrim. If this model does not
     *     correspond to the current scrim, then this request will be ignored.
     * @param animate Whether the scrim should animate and fade out.
     * @param duration Duration for animation in milliseconds.
     */
    public void hideScrim(PropertyModel model, boolean animate, int duration) {
        @Nullable ScrimCoordinator coordinator = mModelToScrim.get(model);
        if (coordinator == null) return;
        coordinator.hideScrim(animate, duration);
    }

    /**
     * Forces the current scrim fade animation to complete if one is running.
     *
     * @param model The model used to show/identify the current scrim. If this model does not
     *     correspond to the current scrim, then this request will be ignored.
     */
    public void forceAnimationToFinish(PropertyModel model) {
        @Nullable ScrimCoordinator coordinator = mModelToScrim.get(model);
        if (coordinator == null) return;
        coordinator.forceAnimationToFinish();
    }

    /**
     * Manually set the alpha for the scrim. This is exposed as part of the public API and should
     * not be called as part of animations as it cancels the currently running one.
     *
     * @param alpha The alpha in range [0, 1].
     * @param model The model used to show/identify the current scrim. If this model does not
     *     correspond to the current scrim, then this request will be ignored.
     */
    public void setAlpha(float alpha, PropertyModel model) {
        @Nullable ScrimCoordinator coordinator = mModelToScrim.get(model);
        if (coordinator == null) return;
        coordinator.setAlpha(alpha);
    }

    /**
     * Changes the target color of a full scrim, though the current color being applied will still
     * be subject to the current alpha. Safe to call this during animations.
     *
     * @param scrimColor The color to set the scrim to.
     * @param model The model used to show/identify the current scrim. If this model does not
     *     correspond to the current scrim, then this request will be ignored.
     */
    public void setScrimColor(@ColorInt int scrimColor, PropertyModel model) {
        @Nullable ScrimCoordinator coordinator = mModelToScrim.get(model);
        if (coordinator == null) return;
        coordinator.setScrimColor(scrimColor);
    }

    private void pruneHiddenScrims(boolean ignoredVisibility) {
        Iterator<ScrimCoordinator> iterator = mModelToScrim.values().iterator();
        while (iterator.hasNext()) {
            ScrimCoordinator coordinator = iterator.next();
            if (!coordinator.isShowingScrim()) {
                iterator.remove();
                destroyScrim(coordinator);
            }
        }
        mScrimVisibilitySupplier.set(!mModelToScrim.isEmpty());
    }

    private void destroyScrim(ScrimCoordinator coordinator) {
        coordinator.removeObserver(mOnScrimVisibilityChanged);
        coordinator.getStatusBarColorSupplier().removeObserver(mOnStatusBarColorChanged);
        coordinator.getNavigationBarColorSupplier().removeObserver(mOnNavBarColorChanged);
        coordinator.destroy();
    }

    private void updateStatusBarColor(@ColorInt int ignoredColor) {
        updateColorSupplier(ScrimCoordinator::getStatusBarColorSupplier, mStatusBarColorSupplier);
    }

    private void updateNavBarColor(@ColorInt int ignoredColor) {
        updateColorSupplier(
                ScrimCoordinator::getNavigationBarColorSupplier, mNavigationBarColorSupplier);
    }

    private void updateColorSupplier(
            Function<ScrimCoordinator, ObservableSupplier<Integer>> unwrap,
            ObservableSupplierImpl<Integer> targetSupplier) {
        @ColorInt int color = Color.TRANSPARENT;
        for (ScrimCoordinator coordinator : orderedScrims()) {
            ObservableSupplier<Integer> inputSupplier = unwrap.apply(coordinator);
            color = ColorUtils.compositeColors(inputSupplier.get(), color);
        }
        targetSupplier.set(color);
    }

    /**
     * Creates an ordered list of scrims, based on the order they draw on the screen. This is
     * important for color calculations, because a partially transparent scrim on top of a full
     * opaque scrim will be a mix of the two. While a fully opaque scrim on top of a partially
     * transparent scrim is just the fully opaque scirm's color. This is done on demand instead of
     * using a SortedMap because the scrim views are inserted into the view hierarchy in an async
     * and not fully constant way, though it does also reduce in more computation.
     */
    private List<ScrimCoordinator> orderedScrims() {
        List<ScrimCoordinator> list = new ArrayList<>(mModelToScrim.values());
        Collections.sort(list, ScrimManager::compareScrimCoordinators);
        return list;
    }

    private static int compareScrimCoordinators(ScrimCoordinator c1, ScrimCoordinator c2) {
        // Flip order to get ascending, as smaller indexes are drawn (and we should apply) first.
        return Integer.compare(c1.getIndexInParent(), c2.getIndexInParent());
    }

    public @Nullable ScrimView getViewForTesting() {
        if (mModelToScrim.isEmpty()) {
            return null;
        } else {
            assert mModelToScrim.size() == 1;
            return mModelToScrim.values().iterator().next().getViewForTesting();
        }
    }

    public @Nullable ScrimView getViewForTesting(@Nullable PropertyModel model) {
        if (model == null) return null;
        @Nullable ScrimCoordinator coordinator = mModelToScrim.get(model);
        if (coordinator == null) return null;
        return coordinator.getViewForTesting();
    }

    public void disableAnimationForTesting(boolean disable) {
        mDisableAnimationForTesting = disable;
        for (ScrimCoordinator scrimCoordinator : mModelToScrim.values()) {
            scrimCoordinator.disableAnimationForTesting(disable);
        }
    }

    public boolean areAnimationsRunningForTesting(@Nullable PropertyModel model) {
        if (model == null) return false;
        @Nullable ScrimCoordinator coordinator = mModelToScrim.get(model);
        if (coordinator == null) return false;
        return coordinator.areAnimationsRunningForTesting();
    }
}
