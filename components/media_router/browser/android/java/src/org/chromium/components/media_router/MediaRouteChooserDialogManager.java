// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.media_router;

import android.content.Context;
import android.content.DialogInterface;
import android.os.Bundle;
import android.os.Handler;
import android.view.View;
import android.widget.AdapterView;
import android.widget.ListView;

import androidx.fragment.app.DialogFragment;
import androidx.fragment.app.FragmentManager;
import androidx.mediarouter.app.MediaRouteChooserDialog;
import androidx.mediarouter.app.MediaRouteChooserDialogFragment;
import androidx.mediarouter.media.MediaRouteSelector;
import androidx.mediarouter.media.MediaRouter;

/** Manages the dialog responsible for selecting a {@link MediaSink}. */
public class MediaRouteChooserDialogManager extends BaseMediaRouteDialogManager {
    private static final String DIALOG_FRAGMENT_TAG =
            "android.support.v7.mediarouter:MediaRouteChooserDialogFragment";

    public MediaRouteChooserDialogManager(
            String sourceId, MediaRouteSelector routeSelector, MediaRouteDialogDelegate delegate) {
        super(sourceId, routeSelector, delegate);
    }

    /** Fragment implementation for MediaRouteChooserDialogManager. */
    public static class Fragment extends MediaRouteChooserDialogFragment {
        private final Handler mHandler = new Handler();
        private final SystemVisibilitySaver mVisibilitySaver = new SystemVisibilitySaver();
        private BaseMediaRouteDialogManager mManager;
        private boolean mIsSinkSelected;

        public Fragment() {
            mHandler.post(
                    new Runnable() {
                        @Override
                        public void run() {
                            Fragment.this.dismiss();
                        }
                    });
        }

        public Fragment(BaseMediaRouteDialogManager manager) {
            mManager = manager;
        }

        @Override
        public MediaRouteChooserDialog onCreateChooserDialog(
                Context context, Bundle savedInstanceState) {
            MediaRouteChooserDialog dialog = new DelayedSelectionDialog(context, getTheme());
            dialog.setCanceledOnTouchOutside(true);
            return dialog;
        }

        @Override
        public void onStart() {
            mVisibilitySaver.saveSystemVisibility(getActivity());
            super.onStart();
        }

        @Override
        public void onStop() {
            super.onStop();
            mVisibilitySaver.restoreSystemVisibility(getActivity());
        }

        @Override
        public void onDismiss(DialogInterface dialog) {
            super.onDismiss(dialog);

            if (!mIsSinkSelected) mManager.delegate().onDialogCancelled();
        }

        private class DelayedSelectionDialog extends MediaRouteChooserDialog {
            public DelayedSelectionDialog(Context context) {
                super(context);
            }

            public DelayedSelectionDialog(Context context, int theme) {
                super(context, theme);
            }

            @Override
            public void onCreate(Bundle savedInstanceState) {
                super.onCreate(savedInstanceState);

                ListView listView = (ListView) findViewById(R.id.mr_chooser_list);
                if (listView != null) {
                    listView.setOnItemClickListener(Fragment.this::onItemClick);
                    recordSinkCountWithDelay();
                }
            }

            // The number of discovered sinks is recorded with a three second
            // delay. This is consistent with how sink count is recorded on
            // Chrome desktop.
            private void recordSinkCountWithDelay() {
                Handler handler = new Handler();
                handler.postDelayed(
                        new Runnable() {
                            @Override
                            public void run() {
                                ListView listView = (ListView) findViewById(R.id.mr_chooser_list);
                                if (listView != null) {
                                    MediaRouteUmaRecorder.recordDeviceCountWithDelay(
                                            listView.getCount());
                                }
                            }
                        },
                        3000);
            }
        }

        private void onItemClick(AdapterView<?> parent, View view, int position, long id) {
            MediaRouter.RouteInfo routeInfo =
                    (MediaRouter.RouteInfo) parent.getItemAtPosition(position);
            if (routeInfo != null && routeInfo.isEnabled()) {
                MediaSink newSink = MediaSink.fromRoute(routeInfo);

                // When a item is clicked, the route is not selected right away. Instead, the route
                // selection is postponed to the actual session launch.
                mManager.delegate().onSinkSelected(mManager.sourceId(), newSink);
                mIsSinkSelected = true;

                dismiss();
            }
        }
    }

    @Override
    protected DialogFragment openDialogInternal(FragmentManager fm) {
        if (fm.findFragmentByTag(DIALOG_FRAGMENT_TAG) != null) return null;

        Fragment fragment = new Fragment(this);
        fragment.setRouteSelector(routeSelector());
        fragment.show(fm, DIALOG_FRAGMENT_TAG);
        fm.executePendingTransactions();

        return fragment;
    }
}
