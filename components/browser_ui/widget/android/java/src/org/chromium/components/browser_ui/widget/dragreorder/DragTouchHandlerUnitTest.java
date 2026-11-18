// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.widget.dragreorder;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;

import android.app.Activity;
import android.view.View.MeasureSpec;
import android.widget.FrameLayout;
import android.widget.TextView;

import androidx.recyclerview.widget.ItemTouchHelper;
import androidx.recyclerview.widget.LinearLayoutManager;
import androidx.recyclerview.widget.RecyclerView;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.components.browser_ui.widget.R;
import org.chromium.components.browser_ui.widget.dragreorder.DragTouchHandler.DragListener;
import org.chromium.components.browser_ui.widget.dragreorder.DragTouchHandler.DraggabilityProvider;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModel.WritableBooleanPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;

/** Unit tests for {@link DragTouchHandler}. */
@RunWith(BaseRobolectricTestRunner.class)
public class DragTouchHandlerUnitTest {
    private static final int TYPE_NORMAL = 0;
    private static final int TYPE_DRAGGABLE = 1;

    private static final WritableObjectPropertyKey<String> TITLE =
            new WritableObjectPropertyKey<>();
    private static final WritableBooleanPropertyKey DRAGGABLE_ONLY_KEY =
            new WritableBooleanPropertyKey();
    private static final PropertyKey[] NORMAL_KEYS = {TITLE};
    private static final PropertyKey[] DRAGGABLE_KEYS = {TITLE, DRAGGABLE_ONLY_KEY};

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private DragListener mDragListener;

    private Activity mActivity;
    private ModelList mModelList;
    private DragTouchHandler mDragTouchHandler;
    private DragReorderableRecyclerViewAdapter mAdapter;
    private RecyclerView mRecyclerView;

    @Before
    public void setUp() {
        mActivity = Robolectric.buildActivity(Activity.class).setup().get();
        mActivity.setTheme(R.style.Theme_BrowserUI_DayNight);

        mModelList = new ModelList();
        mDragTouchHandler = new DragTouchHandler(mActivity, mModelList);
        mDragTouchHandler.addDragListener(mDragListener);
        mAdapter = new DragReorderableRecyclerViewAdapter(mActivity, mModelList, mDragTouchHandler);

        DraggabilityProvider draggabilityProvider =
                new DraggabilityProvider() {
                    @Override
                    public boolean isActivelyDraggable(PropertyModel propertyModel) {
                        return propertyModel.get(DRAGGABLE_ONLY_KEY);
                    }

                    @Override
                    public boolean isPassivelyDraggable(PropertyModel propertyModel) {
                        return propertyModel.get(DRAGGABLE_ONLY_KEY);
                    }
                };

        mAdapter.registerType(
                TYPE_NORMAL, parent -> new TextView(mActivity), (model, view, key) -> {});
        mAdapter.registerDraggableType(
                TYPE_DRAGGABLE,
                parent -> new TextView(mActivity),
                (model, view, key) -> {},
                (holder, itemTouchHelper) -> {},
                draggabilityProvider);

        mRecyclerView = new RecyclerView(mActivity);
        mRecyclerView.setLayoutManager(new LinearLayoutManager(mActivity));
        mRecyclerView.setAdapter(mAdapter);
        mActivity.setContentView(mRecyclerView);
        mAdapter.enableDrag();
    }

    private ListItem buildDraggableItem(String title) {
        PropertyModel model =
                new PropertyModel.Builder(DRAGGABLE_KEYS)
                        .with(TITLE, title)
                        .with(DRAGGABLE_ONLY_KEY, /* value= */ true)
                        .build();
        return new ListItem(TYPE_DRAGGABLE, model);
    }

    private ListItem buildNormalItem(String title) {
        PropertyModel model = new PropertyModel.Builder(NORMAL_KEYS).with(TITLE, title).build();
        return new ListItem(TYPE_NORMAL, model);
    }

    private void layoutRecyclerView() {
        int spec = MeasureSpec.makeMeasureSpec(/* size= */ 500, MeasureSpec.EXACTLY);
        mRecyclerView.measure(spec, spec);
        mRecyclerView.layout(/* l= */ 0, /* t= */ 0, /* r= */ 500, /* b= */ 500);
    }

    @Test
    public void testIsDraggableHelper_GuardsDisabledDetachedAndMismatchedViewType() {
        mModelList.add(buildDraggableItem("item_0"));
        layoutRecyclerView();

        RecyclerView.ViewHolder boundHolder =
                mRecyclerView.findViewHolderForAdapterPosition(/* position= */ 0);
        assertNotNull(boundHolder);
        assertTrue(mDragTouchHandler.isActivelyDraggable(boundHolder));
        assertTrue(mDragTouchHandler.isPassivelyDraggable(boundHolder));

        // Disabled drag returns false.
        mDragTouchHandler.setDragEnabled(/* enabled= */ false);
        assertFalse(mDragTouchHandler.isActivelyDraggable(boundHolder));
        assertFalse(mDragTouchHandler.isPassivelyDraggable(boundHolder));
        mDragTouchHandler.setDragEnabled(/* enabled= */ true);

        // Unbound ViewHolder with a registered viewType (NO_POSITION) returns false.
        RecyclerView.ViewHolder detachedHolder =
                mAdapter.createViewHolder(mRecyclerView, TYPE_DRAGGABLE);
        assertEquals(TYPE_DRAGGABLE, detachedHolder.getItemViewType());
        assertEquals(RecyclerView.NO_POSITION, detachedHolder.getBindingAdapterPosition());
        assertFalse(mDragTouchHandler.isActivelyDraggable(detachedHolder));
        assertFalse(mDragTouchHandler.isPassivelyDraggable(detachedHolder));

        // Replacing mModelList[0] with a different viewType before layout must not pass the
        // incompatible PropertyModel (which lacks DRAGGABLE_ONLY_KEY) to draggabilityProvider.
        mModelList.update(/* index= */ 0, buildNormalItem("normal_0"));
        assertEquals(0, boundHolder.getBindingAdapterPosition());
        assertEquals(TYPE_DRAGGABLE, boundHolder.getItemViewType());
        assertFalse(mDragTouchHandler.isActivelyDraggable(boundHolder));
        assertFalse(mDragTouchHandler.isPassivelyDraggable(boundHolder));
    }

    @Test
    public void testOnMove_GuardsDisabledAndDetachedViewHolders() {
        mModelList.add(buildDraggableItem("item_0"));
        mModelList.add(buildDraggableItem("item_1"));
        layoutRecyclerView();

        RecyclerView.ViewHolder holder0 =
                mRecyclerView.findViewHolderForAdapterPosition(/* position= */ 0);
        RecyclerView.ViewHolder holder1 =
                mRecyclerView.findViewHolderForAdapterPosition(/* position= */ 1);
        assertNotNull(holder0);
        assertNotNull(holder1);

        // Disabled drag rejects move.
        mDragTouchHandler.setDragEnabled(/* enabled= */ false);
        assertFalse(mDragTouchHandler.onMove(mRecyclerView, holder0, holder1));
        assertEquals("item_0", mModelList.get(0).model.get(TITLE));

        // Detached ViewHolder (NO_POSITION) rejects move.
        mDragTouchHandler.setDragEnabled(/* enabled= */ true);
        RecyclerView.ViewHolder detachedHolder =
                mAdapter.createViewHolder(mRecyclerView, TYPE_DRAGGABLE);
        assertFalse(mDragTouchHandler.onMove(mRecyclerView, detachedHolder, holder1));
        assertFalse(mDragTouchHandler.onMove(mRecyclerView, holder0, detachedHolder));
        assertEquals("item_0", mModelList.get(0).model.get(TITLE));

        // Valid enabled move succeeds.
        assertTrue(mDragTouchHandler.onMove(mRecyclerView, holder0, holder1));
        assertEquals("item_1", mModelList.get(0).model.get(TITLE));
        assertEquals("item_0", mModelList.get(1).model.get(TITLE));
    }

    @Test
    public void testClearView_GuardsDisabledDetachedAndPostDisableBeforeCallback() {
        mModelList.add(buildDraggableItem("item_0"));
        mModelList.add(buildDraggableItem("item_1"));
        layoutRecyclerView();

        RecyclerView.ViewHolder holder0 =
                mRecyclerView.findViewHolderForAdapterPosition(/* position= */ 0);
        RecyclerView.ViewHolder holder1 =
                mRecyclerView.findViewHolderForAdapterPosition(/* position= */ 1);
        assertNotNull(holder0);
        assertNotNull(holder1);

        // 1. Detached ViewHolder on clearView does not post onSwap.
        mDragTouchHandler.onSelectedChanged(holder0, ItemTouchHelper.ACTION_STATE_DRAG);
        mModelList.removeAt(/* position= */ 0);
        assertEquals(RecyclerView.NO_POSITION, holder0.getBindingAdapterPosition());
        mDragTouchHandler.clearView(mRecyclerView, holder0);
        ShadowLooper.runUiThreadTasks();
        verify(mDragListener, never()).onSwap(anyInt());

        // Reset 2 items.
        mModelList.clear();
        mModelList.add(buildDraggableItem("item_0"));
        mModelList.add(buildDraggableItem("item_1"));
        layoutRecyclerView();
        holder0 = mRecyclerView.findViewHolderForAdapterPosition(/* position= */ 0);
        holder1 = mRecyclerView.findViewHolderForAdapterPosition(/* position= */ 1);
        assertNotNull(holder0);
        assertNotNull(holder1);

        // 2. Disabling drag after clearView posts onSwap (before posted Runnable runs) suppresses
        // onSwap.
        ShadowLooper.pauseMainLooper();
        mDragTouchHandler.onSelectedChanged(holder0, ItemTouchHelper.ACTION_STATE_DRAG);
        assertTrue(mDragTouchHandler.onMove(mRecyclerView, holder0, holder1));
        layoutRecyclerView();
        mDragTouchHandler.clearView(mRecyclerView, holder0);
        mDragTouchHandler.setDragEnabled(/* enabled= */ false);
        ShadowLooper.idleMainLooper();
        verify(mDragListener, never()).onSwap(anyInt());

        // 3. Detaching RecyclerView after clearView posts onSwap suppresses onSwap.
        mDragTouchHandler.setDragEnabled(/* enabled= */ true);
        ShadowLooper.pauseMainLooper();
        mDragTouchHandler.onSelectedChanged(holder0, ItemTouchHelper.ACTION_STATE_DRAG);
        assertTrue(mDragTouchHandler.onMove(mRecyclerView, holder0, holder1));
        layoutRecyclerView();
        mDragTouchHandler.clearView(mRecyclerView, holder0);
        mActivity.setContentView(new FrameLayout(mActivity));
        ShadowLooper.idleMainLooper();
        verify(mDragListener, never()).onSwap(anyInt());

        // 4. Positive control: attached + drag enabled invokes onSwap with new binding position.
        mActivity.setContentView(mRecyclerView);
        layoutRecyclerView();
        mDragTouchHandler.onSelectedChanged(holder0, ItemTouchHelper.ACTION_STATE_DRAG);
        assertTrue(mDragTouchHandler.onMove(mRecyclerView, holder0, holder1));
        layoutRecyclerView();
        mDragTouchHandler.clearView(mRecyclerView, holder0);
        ShadowLooper.runUiThreadTasks();
        verify(mDragListener).onSwap(/* targetIndex= */ 1);
    }
}
