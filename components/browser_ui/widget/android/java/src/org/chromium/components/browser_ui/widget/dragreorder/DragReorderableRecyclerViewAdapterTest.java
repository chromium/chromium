// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.widget.dragreorder;

import static org.hamcrest.CoreMatchers.is;
import static org.hamcrest.MatcherAssert.assertThat;

import android.view.View;
import android.view.ViewGroup;
import android.widget.TextView;

import androidx.annotation.IntDef;
import androidx.recyclerview.widget.LinearLayoutManager;
import androidx.recyclerview.widget.RecyclerView;
import androidx.test.filters.SmallTest;

import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.components.browser_ui.widget.dragreorder.DragReorderableRecyclerViewAdapter.DragBinder;
import org.chromium.components.browser_ui.widget.dragreorder.DragReorderableRecyclerViewAdapter.DraggabilityProvider;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.MVCListAdapter.ViewBuilder;
import org.chromium.ui.modelutil.ModelListAdapter;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModel.WritableIntPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor.ViewBinder;
import org.chromium.ui.test.util.BlankUiTestActivityTestCase;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/** Tests to ensure/validate {@link DragReorderableRecyclerViewAdapter} behavior. */
@RunWith(ChromeJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
public class DragReorderableRecyclerViewAdapterTest extends BlankUiTestActivityTestCase {
    static final WritableObjectPropertyKey<String> TITLE = new WritableObjectPropertyKey<>();
    static final WritableIntPropertyKey TYPE = new WritableIntPropertyKey();
    static final PropertyKey[] ALL_KEYS = {TITLE, TYPE};

    @IntDef({Type.NORMAL, Type.DRAGGABLE, Type.PASSIVELY_DRAGGABLE})
    @Retention(RetentionPolicy.SOURCE)
    public @interface Type {
        int NORMAL = 0;
        int DRAGGABLE = 1;
        int PASSIVELY_DRAGGABLE = 2;
    }

    private ModelList mModelList;
    private DragReorderableRecyclerViewAdapter mAdapter;
    private RecyclerView mRecyclerView;

    @Override
    public void setUpTest() throws Exception {
        super.setUpTest();

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mRecyclerView = new RecyclerView(getActivity());
                    RecyclerView.LayoutParams params =
                            new RecyclerView.LayoutParams(
                                    RecyclerView.LayoutParams.MATCH_PARENT,
                                    RecyclerView.LayoutParams.MATCH_PARENT);
                    mRecyclerView.setLayoutParams(params);
                    mRecyclerView.setLayoutManager(
                            new LinearLayoutManager(
                                    mRecyclerView.getContext(),
                                    LinearLayoutManager.VERTICAL,
                                    false));
                    mRecyclerView.setAdapter(createAdapter());
                    getActivity().setContentView(mRecyclerView);

                    mAdapter.enableDrag();
                });
    }

    @Override
    public void tearDownTest() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mModelList.clear();
                });
    }

    private DragReorderableRecyclerViewAdapter createAdapter() {
        mModelList = new ModelList();
        mAdapter = new DragReorderableRecyclerViewAdapter(getActivity(), mModelList);

        ViewBuilder<View> viewBuilder = (parent) -> createListItemView();
        ViewBinder<PropertyModel, View, PropertyKey> viewBinder =
                (PropertyModel model, View view, PropertyKey key) -> {
                    if (key == TITLE) {
                        TextView tv = (TextView) view;
                        tv.setText(model.get(TITLE));
                    }
                };
        DragBinder dragBinder = (vh, it) -> {};
        DraggabilityProvider draggabilityProvider =
                new DraggabilityProvider() {
                    @Override
                    public boolean isActivelyDraggable(PropertyModel propertyModel) {
                        return propertyModel.get(TYPE) == Type.DRAGGABLE;
                    }

                    @Override
                    public boolean isPassivelyDraggable(PropertyModel propertyModel) {
                        return propertyModel.get(TYPE) == Type.DRAGGABLE
                                || propertyModel.get(TYPE) == Type.PASSIVELY_DRAGGABLE;
                    }
                };

        mAdapter.registerType(Type.NORMAL, viewBuilder, viewBinder);
        mAdapter.registerDraggableType(
                Type.DRAGGABLE, viewBuilder, viewBinder, dragBinder, draggabilityProvider);
        mAdapter.registerDraggableType(
                Type.PASSIVELY_DRAGGABLE,
                viewBuilder,
                viewBinder,
                dragBinder,
                draggabilityProvider);
        mAdapter.enableDrag();
        mAdapter.setLongPressDragDelegate(this::isLongPressDragEnabled);

        return mAdapter;
    }

    private View createListItemView() {
        TextView tv = new TextView(getActivity());
        tv.setLayoutParams(
                new ViewGroup.LayoutParams(
                        ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT));
        return tv;
    }

    private boolean isLongPressDragEnabled() {
        return false;
    }

    private PropertyModel createPropertyModel(String title, @Type int type) {
        return new PropertyModel.Builder(ALL_KEYS).with(TITLE, title).with(TYPE, type).build();
    }

    private ListItem buildListItem(String title, @Type int type) {
        return new ModelListAdapter.ListItem(type, createPropertyModel(title, type));
    }

    @Test
    @SmallTest
    public void testDrag_cannotDragOverNormal() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mModelList.add(buildListItem("normal_1", Type.NORMAL));
                    mModelList.add(buildListItem("draggable_1", Type.DRAGGABLE));
                });

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mAdapter.simulateDragForTests(1, 0);
                });

        // The order should remain since you can't drag over a normal view.
        assertThat(mModelList.get(0).type, is(Type.NORMAL));
        assertThat(mModelList.get(1).type, is(Type.DRAGGABLE));
    }

    @Test
    @SmallTest
    public void testDrag_normalOverNormal() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mModelList.add(buildListItem("draggable_1", Type.DRAGGABLE));
                    mModelList.add(buildListItem("draggable_2", Type.DRAGGABLE));
                });

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mAdapter.simulateDragForTests(1, 0);
                });

        // The order should remain since you can't drag over a normal view.
        assertThat(mModelList.get(0).model.get(TITLE), is("draggable_2"));
        assertThat(mModelList.get(1).model.get(TITLE), is("draggable_1"));
    }

    @Test
    @SmallTest
    public void testDrag_normalThroughPassivelyDraggable() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mModelList.add(buildListItem("draggable_1", Type.DRAGGABLE));
                    mModelList.add(
                            buildListItem("passively_draggable_1", Type.PASSIVELY_DRAGGABLE));
                    mModelList.add(buildListItem("draggable_2", Type.DRAGGABLE));
                });

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mAdapter.simulateDragForTests(0, 1);
                });

        assertThat(mModelList.get(0).model.get(TITLE), is("passively_draggable_1"));
        assertThat(mModelList.get(1).model.get(TITLE), is("draggable_1"));
    }
}
