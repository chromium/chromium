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
import org.chromium.chrome.browser.toolbar.extensions.ExtensionActionButtonProperties.ListItemType;
import org.chromium.chrome.browser.ui.browser_window.ChromeAndroidTask;
import org.chromium.chrome.browser.ui.extensions.ExtensionsToolbarBridge;
import org.chromium.chrome.browser.ui.extensions.R;
import org.chromium.components.browser_ui.widget.dragreorder.DragReorderableRecyclerViewAdapter;
import org.chromium.components.browser_ui.widget.dragreorder.DragTouchHandler.DragListener;
import org.chromium.components.browser_ui.widget.dragreorder.DragTouchHandler.DraggabilityProvider;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.listmenu.ListMenuButton;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * Root component for the extension action buttons. Exposes public API for external consumers to
 * interact with the buttons and affect their states.
 */
@NullMarked
public class ExtensionActionListCoordinator implements Destroyable {
    /** Provider for extension action button views. */
    public interface ActionAnchorViewProvider {
        @Nullable View getButtonViewForId(String actionId);
    }

    private final Context mContext;
    private final ExtensionActionListRecyclerView mContainer;
    private final ModelList mModels;
    private final ExtensionActionListMediator mMediator;
    private final DragReorderableRecyclerViewAdapter mAdapter;
    @Nullable private final LifetimeAssert mLifetimeAssert = LifetimeAssert.create(this);

    private boolean mIsDragging;

    public ExtensionActionListCoordinator(
            Context context,
            ExtensionActionListRecyclerView container,
            WindowAndroid windowAndroid,
            ChromeAndroidTask task,
            Profile profile,
            NullableObservableSupplier<Tab> currentTabSupplier,
            ExtensionsToolbarBridge extensionsToolbarBridge,
            ViewGroup rootView) {
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
                        this::getButtonViewForId,
                        extensionsToolbarBridge);

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
                    public boolean isActivelyDraggable(PropertyModel propertyModel) {
                        return true;
                    }

                    @Override
                    public boolean isPassivelyDraggable(PropertyModel propertyModel) {
                        return true;
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

    /** Performs a click on the button for the given action. */
    public void click(String actionId) {
        View view = getButtonViewForId(actionId);
        if (view != null) {
            view.performClick();
        }
    }

    @Nullable
    private View getButtonViewForId(String actionId) {
        for (int i = 0; i < mModels.size(); i++) {
            PropertyModel model = mModels.get(i).model;
            if (actionId.equals(model.get(ExtensionActionButtonProperties.ID))) {
                RecyclerView.ViewHolder holder = mContainer.findViewHolderForAdapterPosition(i);
                if (holder == null) {
                    // TODO(crbug.com/478113313): If the action is unpinned, pop it out to show
                    // action popup.
                    return null;
                }
                return holder.itemView;
            }
        }
        return null;
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
}
