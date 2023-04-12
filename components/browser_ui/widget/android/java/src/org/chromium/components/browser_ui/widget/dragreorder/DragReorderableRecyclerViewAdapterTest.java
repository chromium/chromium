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

import org.chromium.base.test.util.Batch;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.ModelListAdapter;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;
import org.chromium.ui.test.util.BlankUiTestActivityTestCase;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/** Tests to ensure/validate SimpleRecyclerViewAdapter behavior. */
@RunWith(ChromeJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
public class DragReorderableRecyclerViewAdapterTest extends BlankUiTestActivityTestCase {
    static final WritableObjectPropertyKey<String> TITLE = new WritableObjectPropertyKey<>();
    static final PropertyKey[] ALL_KEYS = {TITLE};

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

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mRecyclerView = new RecyclerView(getActivity());
            RecyclerView.LayoutParams params = new RecyclerView.LayoutParams(
                    RecyclerView.LayoutParams.MATCH_PARENT, RecyclerView.LayoutParams.MATCH_PARENT);
            mRecyclerView.setLayoutParams(params);
            mRecyclerView.setLayoutManager(new LinearLayoutManager(
                    mRecyclerView.getContext(), LinearLayoutManager.VERTICAL, false));
            mRecyclerView.setAdapter(createAdapter());
            getActivity().setContentView(mRecyclerView);

            mAdapter.enableDrag();
        });
    }

    DragReorderableRecyclerViewAdapter createAdapter() {
        mModelList = new ModelList();
        mAdapter = new DragReorderableRecyclerViewAdapter(
                getActivity(), mModelList, this::isLongPressDragEnabled);

        mAdapter.registerType(Type.NORMAL, (parent) -> createListItemView(), this::bindView);
        mAdapter.registerDraggableType(
                Type.DRAGGABLE, (parent) -> createListItemView(), this::bindView, (vh, it) -> {});
        mAdapter.registerPassivelyDraggableType(
                Type.PASSIVELY_DRAGGABLE, (parent) -> createListItemView(), this::bindView);
        mAdapter.enableDrag();

        return mAdapter;
    }

    View createListItemView() {
        TextView tv = new TextView(getActivity());
        tv.setLayoutParams(new ViewGroup.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT));
        return tv;
    }

    boolean isLongPressDragEnabled() {
        return false;
    }

    @Override
    public void tearDownTest() {
        TestThreadUtils.runOnUiThreadBlocking(() -> { mModelList.clear(); });
    }

    PropertyModel createPropertyModel(String title) {
        return new PropertyModel.Builder(ALL_KEYS).with(TITLE, title).build();
    }

    void bindView(PropertyModel model, View view, PropertyKey key) {
        if (key == TITLE) {
            TextView tv = (TextView) view;
            tv.setText(model.get(TITLE));
        }
    }

    @Test
    @SmallTest
    public void testDrag_cannotDragOverNormal() {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mModelList.add(
                    new ModelListAdapter.ListItem(Type.NORMAL, createPropertyModel("normal_1")));
            mModelList.add(new ModelListAdapter.ListItem(
                    Type.DRAGGABLE, createPropertyModel("draggable_1")));
        });

        TestThreadUtils.runOnUiThreadBlocking(() -> { mAdapter.simulateDragForTests(1, 0); });

        // The order should remain since you can't drag over a normal view.
        assertThat(mModelList.get(0).type, is(Type.NORMAL));
        assertThat(mModelList.get(1).type, is(Type.DRAGGABLE));
    }

    @Test
    @SmallTest
    public void testDrag_normalOverNormal() {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mModelList.add(new ModelListAdapter.ListItem(
                    Type.DRAGGABLE, createPropertyModel("draggable_1")));
            mModelList.add(new ModelListAdapter.ListItem(
                    Type.DRAGGABLE, createPropertyModel("draggable_2")));
        });

        TestThreadUtils.runOnUiThreadBlocking(() -> { mAdapter.simulateDragForTests(1, 0); });

        // The order should remain since you can't drag over a normal view.
        assertThat(mModelList.get(0).model.get(TITLE), is("draggable_2"));
        assertThat(mModelList.get(1).model.get(TITLE), is("draggable_1"));
    }

    @Test
    @SmallTest
    public void testDrag_normalThroughPassivelyDraggable() {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mModelList.add(new ModelListAdapter.ListItem(
                    Type.DRAGGABLE, createPropertyModel("draggable_1")));
            mModelList.add(new ModelListAdapter.ListItem(
                    Type.PASSIVELY_DRAGGABLE, createPropertyModel("passively_draggable_1")));
            mModelList.add(new ModelListAdapter.ListItem(
                    Type.DRAGGABLE, createPropertyModel("draggable_2")));
        });

        TestThreadUtils.runOnUiThreadBlocking(() -> { mAdapter.simulateDragForTests(0, 1); });

        assertThat(mModelList.get(0).model.get(TITLE), is("passively_draggable_1"));
        assertThat(mModelList.get(1).model.get(TITLE), is("draggable_1"));
    }
}
