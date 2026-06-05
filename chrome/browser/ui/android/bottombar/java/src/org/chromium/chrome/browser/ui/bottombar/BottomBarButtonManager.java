// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.bottombar;

import android.util.SparseArray;
import android.view.View;

import androidx.annotation.IntDef;

import org.chromium.base.Callback;
import org.chromium.base.lifetime.Destroyable;
import org.chromium.base.supplier.NullableObservableSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.ui.actions.ActionId;
import org.chromium.chrome.browser.ui.actions.ActionRegistry;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;
import org.chromium.ui.modelutil.PropertyObservable;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.List;

/**
 * Manages the collection of action buttons in the bottom bar, handling their bindings and
 * visibility updates.
 */
@NullMarked
public class BottomBarButtonManager implements Destroyable {

    @IntDef({ButtonPosition.LEFT, ButtonPosition.CENTER, ButtonPosition.RIGHT})
    @Retention(RetentionPolicy.SOURCE)
    public @interface ButtonPosition {
        int LEFT = -1;
        int CENTER = 0;
        int RIGHT = 1;
    }

    /** Listener for button property changes. */
    public interface Listener {
        /**
         * Called when a specific button's visibility changes. Used for button-specific logic (e.g.,
         * Feature Engagement/IPH tracking).
         */
        void onButtonVisibilityChanged(@ActionId int actionId, boolean visible);

        /**
         * Called once when the overall bottom bar state changes (recomputation complete or a
         * property updated).
         *
         * <p>TODO(crbug.com/508296670): Make this more generic if there are other things we need to
         * listen to.
         */
        void onBottomBarStateChanged(boolean visibilityChanged);
    }

    /** Configuration for an action button. */
    public static class ActionConfig {
        public final @ActionId int actionId;
        public final BottomBarButtonContainer container;
        public final PropertyModelChangeProcessor.ViewBinder<PropertyModel, View, PropertyKey>
                binder;
        public final PropertyModel.WritableBooleanPropertyKey visibilityPropertyKey;

        /**
         * Constructs an ActionConfig.
         *
         * @param actionId The ID of the action.
         * @param container The container view for the button.
         * @param binder The view binder for the button.
         * @param visibilityPropertyKey The property key for visibility in the bottom bar model.
         */
        public ActionConfig(
                @ActionId int actionId,
                BottomBarButtonContainer container,
                PropertyModelChangeProcessor.ViewBinder<PropertyModel, View, PropertyKey> binder,
                PropertyModel.WritableBooleanPropertyKey visibilityPropertyKey) {
            this.actionId = actionId;
            this.container = container;
            this.binder = binder;
            this.visibilityPropertyKey = visibilityPropertyKey;
        }
    }

    /** Represents the binding state of a button in the bottom bar. */
    private static class ButtonBinding {
        private final BottomBarButtonContainer mContainer;
        private final PropertyModelChangeProcessor.ViewBinder<PropertyModel, View, PropertyKey>
                mBinder;
        private final NullableObservableSupplier<PropertyModel> mSupplier;
        private final Callback<@Nullable PropertyModel> mObserver;
        private final @ButtonPosition int mPosition;
        private final PropertyModel.WritableBooleanPropertyKey mVisibilityPropertyKey;

        private @Nullable PropertyModel mModel;
        private @Nullable PropertyModelChangeProcessor<PropertyModel, View, PropertyKey> mMcp;
        private boolean mRequestedVisibility = true;
        private boolean mCachedVisibility;

        /**
         * Constructs a ButtonBinding.
         *
         * @param container The container view for the button.
         * @param binder The view binder for the button.
         * @param supplier Supplier for the action's property model.
         * @param observer Observer for the model supplier.
         * @param position The {@link ButtonPosition} in respect to the center button.
         * @param visibilityPropertyKey The property key for visibility in the bottom bar model.
         */
        private ButtonBinding(
                BottomBarButtonContainer container,
                PropertyModelChangeProcessor.ViewBinder<PropertyModel, View, PropertyKey> binder,
                NullableObservableSupplier<PropertyModel> supplier,
                Callback<@Nullable PropertyModel> observer,
                @ButtonPosition int position,
                PropertyModel.WritableBooleanPropertyKey visibilityPropertyKey) {
            mContainer = container;
            mBinder = binder;
            mSupplier = supplier;
            mObserver = observer;
            mPosition = position;
            mVisibilityPropertyKey = visibilityPropertyKey;
        }
    }

    private final PropertyObservable.PropertyObserver<PropertyKey> mModelObserver =
            this::onModelPropertyChanged;
    private final SparseArray<ButtonBinding> mButtons = new SparseArray<>();
    private final PropertyModel mBottomBarModel;

    private @Nullable Listener mListener;
    private boolean mHasCenteredButton;

    /**
     * Constructs a BottomBarButtonManager.
     *
     * @param configs The list of action configurations. **MUST be ordered from left to right** as
     *     they appear in the visual layout for the centering logic to work correctly.
     * @param actionRegistry The registry to get action models from.
     * @param bottomBarModel The model for the bottom bar.
     * @param centerActionId The ID of the action that acts as the center button.
     */
    public BottomBarButtonManager(
            List<ActionConfig> configs,
            ActionRegistry actionRegistry,
            PropertyModel bottomBarModel,
            int centerActionId) {
        mBottomBarModel = bottomBarModel;

        boolean foundCenter = false;
        for (int i = 0; i < configs.size(); i++) {
            ActionConfig config = configs.get(i);
            @ButtonPosition int position;
            if (config.actionId == centerActionId) {
                position = ButtonPosition.CENTER;
                foundCenter = true;
            } else if (!foundCenter) {
                position = ButtonPosition.LEFT;
            } else {
                position = ButtonPosition.RIGHT;
            }

            registerAction(
                    config.actionId,
                    actionRegistry.get(config.actionId),
                    config.container,
                    config.binder,
                    position,
                    config.visibilityPropertyKey);
        }
        assert foundCenter : "Center action not found in configs";
    }

    /**
     * Sets the listener for button property changes.
     *
     * @param listener The listener to set.
     */
    public void setListener(Listener listener) {
        assert mListener == null : "Listener should only be set once";
        mListener = listener;
        for (int i = 0; i < mButtons.size(); i++) {
            @ActionId int actionId = mButtons.keyAt(i);
            ButtonBinding state = mButtons.valueAt(i);
            if (state.mCachedVisibility) {
                listener.onButtonVisibilityChanged(actionId, true);
            }
        }
        listener.onBottomBarStateChanged(/* visibilityChanged= */ true);
    }

    /**
     * Sets the requested visibility for a specific action button.
     *
     * @param actionId The ID of the action.
     * @param visible True to request visibility, false to hide.
     */
    public void setButtonVisibility(@ActionId int actionId, boolean visible) {
        ButtonBinding state = mButtons.get(actionId);
        assert state != null : "Action not registered: " + actionId;

        state.mRequestedVisibility = visible;
        recomputeState();
    }

    /**
     * Checks if the bottom bar currently has a centered button configuration.
     *
     * @return True if the visible buttons are balanced around the center.
     */
    public boolean hasCenteredButton() {
        return mHasCenteredButton;
    }

    /**
     * Registers an action with a specific position.
     *
     * @param actionId The ID of the action.
     * @param supplier Supplier for the action's property model.
     * @param container The container view for the button.
     * @param binder The view binder for the button.
     * @param position The relative position of the button.
     * @param visibilityPropertyKey The property key for visibility in the bottom bar model.
     */
    private void registerAction(
            @ActionId int actionId,
            NullableObservableSupplier<PropertyModel> supplier,
            BottomBarButtonContainer container,
            PropertyModelChangeProcessor.ViewBinder<PropertyModel, View, PropertyKey> binder,
            @ButtonPosition int position,
            PropertyModel.WritableBooleanPropertyKey visibilityPropertyKey) {
        assert mButtons.indexOfKey(actionId) < 0 : "Action already registered: " + actionId;

        Callback<@Nullable PropertyModel> observer = model -> onModelChanged(actionId, model);

        ButtonBinding state =
                new ButtonBinding(
                        container, binder, supplier, observer, position, visibilityPropertyKey);
        mButtons.put(actionId, state);

        supplier.addSyncObserverAndCallIfNonNull(observer);
    }

    /**
     * Recomputes the visibility and centering state of the buttons.
     *
     * <p>The centering logic works by assigning a position score to each button: - Buttons to the
     * left of the center button have score -1. - The center button has score 0. - Buttons to the
     * right have score 1.
     *
     * <p>If the sum of scores of all visible buttons is 0, the bar is considered centered (i.e.,
     * equal number of visible buttons on left and right).
     */
    private void recomputeState() {
        // We run in two separate passes so that all internal manager state (like centering)
        // is fully calculated and consistent before we dispatch events to the listener.

        // Update model properties and calculate centering.
        int balance = 0;
        for (int i = 0; i < mButtons.size(); i++) {
            ButtonBinding state = mButtons.valueAt(i);
            boolean visible = isVisible(state);
            mBottomBarModel.set(state.mVisibilityPropertyKey, visible);
            if (visible) {
                balance += state.mPosition;
            }
        }
        mHasCenteredButton = balance == ButtonPosition.CENTER;

        // Update cached visibility and dispatch button-specific events.
        boolean anyVisibilityChanged = false;
        for (int i = 0; i < mButtons.size(); i++) {
            @ActionId int actionId = mButtons.keyAt(i);
            ButtonBinding state = mButtons.valueAt(i);
            boolean visible = isVisible(state);
            if (state.mCachedVisibility != visible) {
                state.mCachedVisibility = visible;
                anyVisibilityChanged = true;
                if (mListener != null) {
                    mListener.onButtonVisibilityChanged(actionId, visible);
                }
            }
        }

        // Notify mediator to handle layout/compositor invalidation.
        if (mListener != null) {
            mListener.onBottomBarStateChanged(anyVisibilityChanged);
        }
    }

    private boolean isVisible(ButtonBinding state) {
        return state.mModel != null && state.mRequestedVisibility;
    }

    private void onModelChanged(@ActionId int actionId, @Nullable PropertyModel model) {
        ButtonBinding state = mButtons.get(actionId);
        assert state != null : "State not found for action: " + actionId;

        cleanupActionObservation(state);

        state.mModel = model;

        if (model != null) {
            state.mMcp =
                    PropertyModelChangeProcessor.create(model, state.mContainer, state.mBinder);
            model.addObserver(mModelObserver);
        }
        recomputeState();
    }

    private void cleanupActionObservation(ButtonBinding state) {
        if (state.mMcp != null) {
            state.mMcp.destroy();
            state.mMcp = null;
        }

        if (state.mModel != null) {
            state.mModel.removeObserver(mModelObserver);
            state.mModel = null;
        }
    }

    private void onModelPropertyChanged(
            PropertyObservable<PropertyKey> source, PropertyKey propertyKey) {
        if (mListener != null) {
            mListener.onBottomBarStateChanged(/* visibilityChanged= */ false);
        }
    }

    @Override
    public void destroy() {
        for (int i = 0; i < mButtons.size(); i++) {
            ButtonBinding state = mButtons.valueAt(i);
            state.mSupplier.removeObserver(state.mObserver);
            cleanupActionObservation(state);
        }
        mButtons.clear();
        mListener = null;
    }
}
