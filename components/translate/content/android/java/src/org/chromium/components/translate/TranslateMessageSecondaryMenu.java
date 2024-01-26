// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.translate;

import android.content.Context;
import android.database.DataSetObserver;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.AdapterView;
import android.widget.AdapterView.OnItemClickListener;
import android.widget.ListView;

import org.chromium.components.translate.TranslateMessage.MenuItem;
import org.chromium.ui.UiUtils;
import org.chromium.ui.listmenu.ListMenu;

import java.util.LinkedList;
import java.util.List;

class TranslateMessageSecondaryMenu implements ListMenu, OnItemClickListener {
    @FunctionalInterface
    public static interface Handler {
        public MenuItem[] handleSecondaryMenuItemClicked(MenuItem menuItem);
    }

    private final Handler mHandler;
    private final TranslateMessageSecondaryMenuAdapter mAdapter;
    private final View mContentView;
    private final ListView mListView;
    private final List<Runnable> mClickRunnables;

    public TranslateMessageSecondaryMenu(
            Context context,
            Handler handler,
            DataSetObserver dataSetObserver,
            MenuItem[] menuItems) {
        mHandler = handler;
        mAdapter = new TranslateMessageSecondaryMenuAdapter(context, menuItems);
        // The dataSetObserver *must* be registered on mAdapter before the call to
        // mListView.setAdapter() below, so that the dimensions of the AnchoredPopupWindow are
        // updated before the ListView's DataSetObserver is called, which will update the ListView's
        // appearance.
        mAdapter.registerDataSetObserver(dataSetObserver);

        mContentView = LayoutInflater.from(context).inflate(R.layout.app_menu_layout, null);
        mListView = mContentView.findViewById(R.id.app_menu_list);
        mListView.setAdapter(mAdapter);
        mListView.setDivider(null);
        mListView.setOnItemClickListener(this);

        mClickRunnables = new LinkedList<>();
    }

    @Override
    public void onItemClick(AdapterView<?> parent, View view, int position, long id) {
        MenuItem[] newItems =
                mHandler.handleSecondaryMenuItemClicked((MenuItem) mAdapter.getItem(position));

        if (newItems != null) {
            mAdapter.setMenuItems(newItems);
        } else {
            for (Runnable r : mClickRunnables) {
                r.run();
            }
        }
    }

    @Override
    public View getContentView() {
        return mContentView;
    }

    @Override
    public void addContentViewClickRunnable(Runnable runnable) {
        mClickRunnables.add(runnable);
    }

    @Override
    public int getMaxItemWidth() {
        return UiUtils.computeListAdapterContentDimensions(mAdapter, mListView)[0];
    }
}
