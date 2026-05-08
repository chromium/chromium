// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.extensions;

import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;

import androidx.recyclerview.widget.ItemTouchHelper;
import androidx.recyclerview.widget.RecyclerView;

import org.chromium.base.lifetime.Destroyable;
import org.chromium.base.lifetime.LifetimeAssert;
import org.chromium.base.supplier.NullableObservableSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.toolbar.extensions.ExtensionActionButtonProperties.ListItemType;
import org.chromium.chrome.browser.ui.browser_window.ChromeAndroidTask;
import org.chromium.chrome.browser.ui.extensions.ExtensionsToolbarBridge;
import org.chromium.chrome.browser.ui.toolbar.InvocationSource;
import org.chromium.components.browser_ui.widget.dragreorder.DragReorderableRecyclerViewAdapter;
import org.chromium.components.browser_ui.widget.dragreorder.DragTouchHandler.DragListener;
import org.chromium.components.browser_ui.widget.dragreorder.DragTouchHandler.DraggabilityProvider;
import org.chromium.components.embedder_support.contextmenu.ContextMenuPopulatorFactory;
import org.chromium.content_public.browser.selection.SelectionDropdownMenuDelegate;
import org.chromium.ui.base.ViewUtils;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.listmenu.ListMenuButton;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * Root component for the extension action buttons. Exposes public API for external consumers to
 * interact with the buttons and affect their states.
 */
@NullMarked
public class ExtensionActionListCoordinator implements Destroyable {

    private final Context mContext;
    private final ExtensionActionListRecyclerView mContainer;
    private final ModelList mModels;
    private final ExtensionActionListMediator mMediator;
    private final DragReorderableRecyclerViewAdapter mAdapter;
    @Nullable private final LifetimeAssert mLifetimeAssert = LifetimeAssert.create(this);
    private final RecyclerViewDelegate mRecyclerViewDelegate = new RecyclerViewDelegate();

    private boolean mIsDragging;

    public ExtensionActionListCoordinator(
            Context context,
            ExtensionActionListRecyclerView container,
            WindowAndroid windowAndroid,
            ChromeAndroidTask task,
            Profile profile,
            NullableObservableSupplier<Tab> currentTabSupplier,
            ExtensionsToolbarBridge extensionsToolbarBridge,
            ViewGroup rootView,
            @Nullable ContextMenuPopulatorFactory contextMenuPopulatorFactory,
            @Nullable SelectionDropdownMenuDelegate selectionDropdownMenuDelegate,
            TabModelSelector tabModelSelector,
            ModalDialogManager modalDialogManager) {
        mContext = context;
        mContainer = container;

        mModels = new ModelList();
        mMediator =
                new ExtensionActionListMediator(
                        context,
                        windowAndroid,
                        mModels,
                        task,
                        profile,
                        currentTabSupplier,
                        mRecyclerViewDelegate,
                        extensionsToolbarBridge,
                        contextMenuPopulatorFactory,
                        selectionDropdownMenuDelegate,
                        tabModelSelector,
                        modalDialogManager);

        ExtensionsToolbarDragTouchHandler dragTouchHandler =
                new ExtensionsToolbarDragTouchHandler(context, mModels);
        mAdapter = new DragReorderableRecyclerViewAdapter(context, mModels, dragTouchHandler);

        dragTouchHandler.setDefaultLongPressDragEnabled(false);

        mAdapter.registerDraggableType(
                ListItemType.EXTENSION_ACTION,
                parent ->
                        (ListMenuButton)
                                LayoutInflater.from(context)
                                        .inflate(
                                                R.layout.extension_action_button,
                                                parent,
                                                /* attachToRoot= */ false),
                ExtensionActionButtonViewBinder::bind,
                this::bindDragProperties,
                new DraggabilityProvider() {
                    @Override
                    public boolean isActivelyDraggable(PropertyModel model) {
                        return model.get(ExtensionActionButtonProperties.IS_DRAGGABLE);
                    }

                    @Override
                    public boolean isPassivelyDraggable(PropertyModel model) {
                        return model.get(ExtensionActionButtonProperties.IS_DRAGGABLE);
                    }
                });
        dragTouchHandler.addDragListener(
                new DragListener() {
                    @Override
                    public void onDragStateChange(boolean drag) {
                        mIsDragging = drag;
                    }

                    @Override
                    public void onSwap(int targetIndex) {
                        mMediator.onActionsSwapped(targetIndex);
                    }
                });

        mContainer.setAdapter(mAdapter);
        mContainer.setTransitionRoot(rootView);

        mAdapter.enableDrag();
    }

    @Override
    public void destroy() {
        mAdapter.destroy();
        mMediator.destroy();
        LifetimeAssert.setSafeToGc(mLifetimeAssert, true);
    }

    /**
     * Executes the given action.
     *
     * @param actionId The ID of the action to execute.
     * @param source How this execution was triggered.
     */
    public void executeUserAction(String actionId, @InvocationSource int source) {
        mMediator.executeUserAction(actionId, source);
    }

    @Nullable
    private View getButtonViewForId(String actionId) {
        for (int i = 0; i < mModels.size(); i++) {
            PropertyModel model = mModels.get(i).model;
            if (actionId.equals(model.get(ExtensionActionButtonProperties.ID))) {
                RecyclerView.ViewHolder holder = mContainer.findViewHolderForAdapterPosition(i);
                if (holder == null) {
                    return null;
                }
                return holder.itemView;
            }
        }
        return null;
    }

    /** Returns whether there is a popped out action. */
    public boolean hasPoppedOutAction() {
        return mMediator.hasPoppedOutAction();
    }

    /**
     * Remembers whether we can show the popped out action, but does not update the UI just yet, to
     * avoid refreshing the UI twice. We actually update the UI via {@link fitActionsWithinWidth()},
     * which will be called due to the rest of the action list having a lower priority.
     */
    public int setCanShowPoppedOutAction(int availableWidth) {
        return mMediator.setCanShowPoppedOutAction(availableWidth);
    }

    /**
     * Updates the list of displayed actions to fit within the provided width constraint.
     *
     * @param availableWidth The maximum width available for the action list in pixels.
     * @return The actual width of the action list after fitting the items.
     */
    public int fitActionsWithinWidth(int availableWidth) {
        if (mIsDragging) {
            // This method gets called when icons are being dragged, but we don't want width update
            // to happen then.
            return mContainer.getWidth();
        }

        mMediator.fitActionsWithinWidth(availableWidth);
        return mContainer.getWidth();
    }

    private void bindDragProperties(
            RecyclerView.ViewHolder viewHolder, ItemTouchHelper itemTouchHelper) {
        int position = viewHolder.getBindingAdapterPosition();
        if (position == RecyclerView.NO_POSITION) return;

        PropertyModel model = mModels.get(position).model;

        ExtensionActionDragHelper dragHelper =
                new ExtensionActionDragHelper(mContext, itemTouchHelper, viewHolder);

        model.set(ExtensionActionButtonProperties.DRAG_HELPER, dragHelper);
        model.set(ExtensionActionButtonProperties.TOUCH_LISTENER, dragHelper::onTouch);
    }

    public class RecyclerViewDelegate {
        /**
         * Retrieves the view representing the button for a specific extension action.
         *
         * @param actionId The ID of the action.
         * @return The {@link View} of the given action ID, or {@code null} if no such view exists
         *     in the current layout.
         */
        public @Nullable View getButtonViewForId(String actionId) {
            return ExtensionActionListCoordinator.this.getButtonViewForId(actionId);
        }

        /**
         * Adds a {@link Runnable} to the RecyclerView container that will be executed after a
         * layout phase or animation cycle completes.
         *
         * <p>Specifically, the runnable will execute on the next layout pass if there are no
         * animations currently running. If an animation is in progress, the runnable will be
         * deferred and executed as soon as the animation ends. The callback is not guaranteed to be
         * executed because it can be cleared by {@link clearOnAnimationFinishedRunnables()}.
         *
         * @param runnable The {@link Runnable} to execute once layouts/animations are settled.
         */
        public void addOnAnimationsFinishedRunnable(Runnable runnable) {
            mContainer.addOnAnimationsFinishedRunnable(runnable);
        }

        /** Clears all pending animations-finished runnables from {@link mContainer}. */
        public void clearOnAnimationsFinishedRunnables() {
            mContainer.clearOnAnimationsFinishedRunnables();
        }

        /** Requests a layout pass on the underlying RecyclerView container. */
        public void requestLayoutWithViewUtils() {
            ViewUtils.requestLayout(
                    mContainer, "RecyclerViewDelegate.requestLayoutWithViewUtils()");
        }
    }
}
