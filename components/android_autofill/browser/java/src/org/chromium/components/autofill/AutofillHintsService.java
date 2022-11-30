// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.autofill;

import android.os.IBinder;

import org.chromium.base.Log;
import org.chromium.components.autofill_public.IAutofillHintsService;
import org.chromium.components.autofill_public.IViewTypeCallback;
import org.chromium.components.autofill_public.ViewType;

import java.util.List;

/**
 * This class is used to talk to autofill service about the view type.
 */
public class AutofillHintsService {
    private static final String TAG = "AutofillHintsService";

    public AutofillHintsService() {
        mBinder = new IAutofillHintsService.Stub() {
            @Override
            public void registerViewTypeCallback(IViewTypeCallback callback) {
                mCallback = callback;
                if (mUnsentViewTypes != null) {
                    invokeOnViewTypeAvailable();
                } else if (mQueryFailed != null) {
                    invokeOnQueryFailed();
                }
            }
        };
    }

    public IBinder getBinder() {
        return mBinder;
    }

    public void onViewTypeAvailable(List<ViewType> viewTypes) {
        if (mUnsentViewTypes != null) return;
        mUnsentViewTypes = viewTypes;
        if (mCallback == null) return;
        invokeOnViewTypeAvailable();
    }

    public void onQueryFailed() {
        if (mQueryFailed != null) return;
        mQueryFailed = Boolean.TRUE;
        if (mCallback == null) return;
        invokeOnQueryFailed();
    }

    private void invokeOnViewTypeAvailable() {
        try {
            mCallback.onViewTypeAvailable(mUnsentViewTypes);
        } catch (Exception e) {
            Log.e(TAG, "onViewTypeAvailable ", e);
        }
    }

    private void invokeOnQueryFailed() {
        try {
            mCallback.onQueryFailed();
        } catch (Exception e) {
            Log.e(TAG, "onQueryFailed ", e);
        }
    }

    private IAutofillHintsService.Stub mBinder;
    private IViewTypeCallback mCallback;
    private List<ViewType> mUnsentViewTypes;
    private Boolean mQueryFailed;
}
