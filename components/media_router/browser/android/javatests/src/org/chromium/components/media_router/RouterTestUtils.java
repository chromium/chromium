// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.media_router;

import android.app.Dialog;
import android.view.View;

import androidx.fragment.app.DialogFragment;
import androidx.fragment.app.FragmentManager;

import org.hamcrest.Matchers;

import org.chromium.base.Log;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.CriteriaNotSatisfiedException;

import java.util.ArrayList;
import java.util.concurrent.Callable;

/** Test utils for MediaRouter. */
public class RouterTestUtils {
    private static final String TAG = "RouterTestUtils";

    private static final int VIEW_TIMEOUT_MS = 2000;
    private static final int VIEW_RETRY_MS = 100;

    public static View waitForRouteButton(
            final FragmentManager fragmentManager, final String chromecastName) {
        return waitForView(
                new Callable<View>() {
                    @Override
                    public View call() {
                        Dialog mediaRouteListDialog = getDialog(fragmentManager);
                        if (mediaRouteListDialog == null) {
                            Log.w(TAG, "Cannot find device selection dialog");
                            return null;
                        }
                        View mediaRouteList =
                                mediaRouteListDialog.findViewById(R.id.mr_chooser_list);
                        if (mediaRouteList == null) {
                            Log.w(TAG, "Cannot find device list");
                            return null;
                        }
                        ArrayList<View> routesWanted = new ArrayList<View>();
                        mediaRouteList.findViewsWithText(
                                routesWanted, chromecastName, View.FIND_VIEWS_WITH_TEXT);
                        if (routesWanted.size() == 0) {
                            Log.w(TAG, "Cannot find wanted device");
                            return null;
                        }
                        Log.i(TAG, "Found wanted device");
                        return routesWanted.get(0);
                    }
                });
    }

    public static Dialog waitForDialog(final FragmentManager fragmentManager) {
        try {
            CriteriaHelper.pollUiThread(
                    () -> {
                        try {
                            Criteria.checkThat(getDialog(fragmentManager), Matchers.notNullValue());
                        } catch (Exception e) {
                            throw new CriteriaNotSatisfiedException(e);
                        }
                    },
                    VIEW_TIMEOUT_MS,
                    VIEW_RETRY_MS);
            return getDialog(fragmentManager);
        } catch (Exception e) {
            return null;
        }
    }

    private static Dialog getDialog(FragmentManager fragmentManager) {
        DialogFragment fragment =
                (DialogFragment)
                        fragmentManager.findFragmentByTag(
                                "android.support.v7.mediarouter:MediaRouteChooserDialogFragment");
        if (fragment == null) return null;
        return fragment.getDialog();
    }

    private static View waitForView(final Callable<View> getViewCallable) {
        try {
            CriteriaHelper.pollUiThread(
                    () -> {
                        try {
                            Criteria.checkThat(getViewCallable.call(), Matchers.notNullValue());
                        } catch (Exception e) {
                            throw new CriteriaNotSatisfiedException(e);
                        }
                    },
                    VIEW_TIMEOUT_MS,
                    VIEW_RETRY_MS);
            return getViewCallable.call();
        } catch (Exception e) {
            return null;
        }
    }
}
