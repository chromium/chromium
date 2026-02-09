// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.layouts;

import org.chromium.base.Callback;
import org.chromium.base.supplier.NonNullObservableSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.layouts.scene_layer.SceneLayer;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor.ViewBinder;
import org.chromium.ui.modelutil.PropertyObservable;

import java.util.Set;

/**
 * A specialized ModelChangeProcessor for compositor. When a new frame is generated, it will bind
 * the whole Model to the View. If everything is idle, it requests another frame on Property
 * changes.
 *
 * @param <V> A view type that extends {@link SceneLayer}.
 */
@NullMarked
public class CompositorModelChangeProcessor<V extends SceneLayer> {
    private final V mView;
    private final PropertyModel mModel;
    private final ViewBinder<PropertyModel, V, @Nullable PropertyKey> mViewBinder;
    private final NonNullObservableSupplier<Long> mFrameSupplier;
    private final Runnable mRequestFrameRunnable;
    private final PropertyObservable.PropertyObserver<PropertyKey> mPropertyObserver;
    private final Callback<Long> mNewFrameCallback;
    private final @Nullable Set<PropertyKey> mExclusions;
    private boolean mViewOutdated;

    /**
     * Construct a new CompositorModelChangeProcessor.
     *
     * @param model The model containing the data to be bound to the view.
     * @param view The view which the model will be bound to.
     * @param viewBinder This is used to bind the model to the view.
     * @param frameSupplier A supplier for the new generated frame.
     * @param exclusions When the value of any of these PropertyKeys change, a new frame will not be
     *     requested. The model will still be bound to the view.
     */
    private CompositorModelChangeProcessor(
            PropertyModel model,
            V view,
            ViewBinder<PropertyModel, V, @Nullable PropertyKey> viewBinder,
            NonNullObservableSupplier<Long> frameSupplier,
            Runnable requestFrameRunnable,
            boolean performInitialBind,
            @Nullable Set<PropertyKey> exclusions) {
        mModel = model;
        mView = view;
        mViewBinder = viewBinder;
        mFrameSupplier = frameSupplier;
        mRequestFrameRunnable = requestFrameRunnable;
        mNewFrameCallback =
                ChromeFeatureList.sMvcUpdateViewWhenModelChanged.isEnabled()
                        ? this::onNewFrameUpdateWhenOutdated
                        : this::onNewFrame;
        mFrameSupplier.addSyncObserverAndPostIfNonNull(mNewFrameCallback);
        mExclusions = exclusions;

        if (performInitialBind) {
            onPropertyChanged(model, null);
        }

        mPropertyObserver = this::onPropertyChanged;
        model.addObserver(mPropertyObserver);
    }

    /**
     * Creates a CompositorModelChangeProcessor observing the given {@code model} and {@code
     * frameSupplier}.
     *
     * @param model The model containing the data to be bound to the view.
     * @param view The view which the model will be bound to.
     * @param viewBinder This is used to bind the model to the view.
     * @param frameSupplier A supplier for the new generated frame.
     * @param performInitialBind Whether the model should be immediately bound to the view.
     * @param exclusions When the value of any of these PropertyKeys change, a new frame will not be
     *     requested. The model will still be bound to the view.
     */
    public static <V extends SceneLayer> CompositorModelChangeProcessor<V> create(
            PropertyModel model,
            V view,
            ViewBinder<PropertyModel, V, @Nullable PropertyKey> viewBinder,
            NonNullObservableSupplier<Long> frameSupplier,
            Runnable requestFrameRunnable,
            boolean performInitialBind,
            @Nullable Set<PropertyKey> exclusions) {
        return new CompositorModelChangeProcessor(
                model,
                view,
                viewBinder,
                frameSupplier,
                requestFrameRunnable,
                performInitialBind,
                exclusions);
    }

    /**
     * Creates a CompositorModelChangeProcessor observing the given {@code model} and {@code
     * frameSupplier}.
     *
     * @param model The model containing the data to be bound to the view.
     * @param view The view which the model will be bound to.
     * @param viewBinder This is used to bind the model to the view.
     * @param frameSupplier A supplier for the new generated frame.
     * @param performInitialBind Whether the model should be immediately bound to the view.
     */
    public static <V extends SceneLayer> CompositorModelChangeProcessor<V> create(
            PropertyModel model,
            V view,
            ViewBinder<PropertyModel, V, @Nullable PropertyKey> viewBinder,
            NonNullObservableSupplier<Long> frameSupplier,
            Runnable requestFrameRunnable,
            boolean performInitialBind) {
        return create(
                model,
                view,
                viewBinder,
                frameSupplier,
                requestFrameRunnable,
                performInitialBind,
                null);
    }

    /**
     * Creates a CompositorModelChangeProcessor observing the given {@code model} and {@code
     * frameSupplier}. The model will be bound to the view initially, and request a new frame.
     *
     * @param model The model containing the data to be bound to the view.
     * @param view The view which the model will be bound to.
     * @param viewBinder This is used to bind the model to the view.
     * @param frameSupplier A supplier for the new generated frame.
     */
    public static <V extends SceneLayer> CompositorModelChangeProcessor<V> create(
            PropertyModel model,
            V view,
            ViewBinder<PropertyModel, V, @Nullable PropertyKey> viewBinder,
            NonNullObservableSupplier<Long> frameSupplier,
            Runnable requestFrameRunnable) {
        return create(model, view, viewBinder, frameSupplier, requestFrameRunnable, true);
    }

    /** Clean up members. */
    public void destroy() {
        mModel.removeObserver(mPropertyObserver);
        mFrameSupplier.removeObserver(mNewFrameCallback);
    }

    private void onNewFrame(Long time) {
        pushUpdate();
    }

    private void onNewFrameUpdateWhenOutdated(Long time) {
        if (mViewOutdated) {
            pushUpdate();
            mViewOutdated = false;
        }
    }

    private void pushUpdate() {
        mViewBinder.bind(mModel, mView, null);
    }

    private void onPropertyChanged(
            PropertyObservable<PropertyKey> model, @Nullable PropertyKey propertyKey) {
        assert model == mModel;
        if (mExclusions != null && mExclusions.contains(propertyKey)) {
            return;
        }
        mViewOutdated = true;

        mRequestFrameRunnable.run();
    }
}
