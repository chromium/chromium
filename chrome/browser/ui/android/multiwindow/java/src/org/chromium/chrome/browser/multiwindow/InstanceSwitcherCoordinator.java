// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.multiwindow;

import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.ListView;

import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.ModelListAdapter;

import java.util.List;

/**
 * Coordinator to construct the instance switcher dialog.
 */
public class InstanceSwitcherCoordinator {
    private final Context mContext;
    private final InstanceSwitcherMediator mMediator;
    private final View mDialogView;

    /**
     * Show instance switcher modal dialog UI.
     * @param context Context to use to build the dialog.
     * @param openAction Action to take when asked to open a chosen instance.
     * @param closeAction Action to take when asked to close a chosen instance.
     * @param instanceInfo List of {@link InstanceInfo} for available Chrome instances.
     */
    public static void showDialog(Context context, InstanceSwitcherMediator.OpenAction openAction,
            InstanceSwitcherMediator.CloseAction closeAction, List<InstanceInfo> instanceInfo) {
        new InstanceSwitcherCoordinator(context, openAction, closeAction).showDialog(instanceInfo);
    }

    private InstanceSwitcherCoordinator(Context context,
            InstanceSwitcherMediator.OpenAction openAction,
            InstanceSwitcherMediator.CloseAction closeAction) {
        mContext = context;
        ModelList modelList = new ModelList();
        mMediator = new InstanceSwitcherMediator(context, modelList, openAction, closeAction);

        ModelListAdapter adapter = new ModelListAdapter(modelList);
        // TODO: Extend modern_list_item_view.xml to replace instance_switcher_item.xml
        adapter.registerType(0,
                parentView
                -> LayoutInflater.from(mContext).inflate(R.layout.instance_switcher_item, null),
                InstanceSwitcherItemViewBinder::bind);
        mDialogView = LayoutInflater.from(context).inflate(R.layout.instance_switcher_dialog, null);
        ListView listView = (ListView) mDialogView.findViewById(R.id.list_view);
        listView.setAdapter(adapter);
    }

    private void showDialog(List<InstanceInfo> items) {
        mMediator.showDialog(mDialogView, items);
    }
}
