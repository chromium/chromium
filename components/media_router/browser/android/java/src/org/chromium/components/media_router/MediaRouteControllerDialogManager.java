// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.media_router;

import android.content.Context;
import android.content.DialogInterface;
import android.os.Bundle;
import android.os.Handler;

import androidx.fragment.app.DialogFragment;
import androidx.fragment.app.FragmentManager;
import androidx.mediarouter.app.MediaRouteControllerDialog;
import androidx.mediarouter.app.MediaRouteControllerDialogFragment;
import androidx.mediarouter.media.MediaRouteSelector;
import androidx.mediarouter.media.MediaRouter;

/** Manages the dialog responsible for controlling an existing media route. */
public class MediaRouteControllerDialogManager extends BaseMediaRouteDialogManager {
    private static final String DIALOG_FRAGMENT_TAG =
            "androidx.mediarouter:MediaRouteControllerDialogFragment";

    private final String mMediaRouteId;

    private final MediaRouter.Callback mCallback =
            new MediaRouter.Callback() {
                @Override
                public void onRouteUnselected(MediaRouter router, MediaRouter.RouteInfo route) {
                    delegate().onRouteClosed(mMediaRouteId);
                }
            };

    public MediaRouteControllerDialogManager(
            String sourceId,
            MediaRouteSelector routeSelector,
            String mediaRouteId,
            MediaRouteDialogDelegate delegate) {
        super(sourceId, routeSelector, delegate);
        mMediaRouteId = mediaRouteId;
    }

    /** Fragment implementation for MediaRouteControllerDialogManager. */
    public static class Fragment extends MediaRouteControllerDialogFragment {
        private final Handler mHandler = new Handler();
        private final SystemVisibilitySaver mVisibilitySaver = new SystemVisibilitySaver();
        private BaseMediaRouteDialogManager mManager;
        private MediaRouter.Callback mCallback;

        public Fragment() {
            mHandler.post(
                    new Runnable() {
                        @Override
                        public void run() {
                            Fragment.this.dismiss();
                        }
                    });
        }

        public Fragment(BaseMediaRouteDialogManager manager, MediaRouter.Callback callback) {
            mManager = manager;
            mCallback = callback;
        }

        @Override
        public MediaRouteControllerDialog onCreateControllerDialog(
                Context context, Bundle savedInstanceState) {
            MediaRouteControllerDialog dialog =
                    super.onCreateControllerDialog(context, savedInstanceState);
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
            if (mManager == null) return;

            mManager.delegate().onDialogCancelled();
            mManager.androidMediaRouter().removeCallback(mCallback);
            mManager.mDialogFragment = null;
        }
    }
    ;

    @Override
    protected DialogFragment openDialogInternal(FragmentManager fm) {
        if (fm.findFragmentByTag(DIALOG_FRAGMENT_TAG) != null) return null;

        Fragment fragment = new Fragment(this, mCallback);
        androidMediaRouter().addCallback(routeSelector(), mCallback);

        fragment.show(fm, DIALOG_FRAGMENT_TAG);
        fm.executePendingTransactions();

        return fragment;
    }
}
