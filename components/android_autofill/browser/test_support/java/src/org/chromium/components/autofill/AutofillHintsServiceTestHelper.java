// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.autofill;

import android.os.IBinder;

import org.chromium.base.test.util.CallbackHelper;
import org.chromium.components.autofill_public.IAutofillHintsService;
import org.chromium.components.autofill_public.IViewTypeCallback;
import org.chromium.components.autofill_public.ViewType;

import java.util.List;

/** This class implements and registers IViewTypeCallback for testing. */
public class AutofillHintsServiceTestHelper {
    public void registerViewTypeService(IBinder binder) throws Exception {
        IAutofillHintsService.Stub.asInterface(binder).registerViewTypeCallback(getBinder());
    }

    private IViewTypeCallback.Stub mBinder =
            new IViewTypeCallback.Stub() {
                @Override
                public void onViewTypeAvailable(List<ViewType> viewTypeList) {
                    mViewTypeList = viewTypeList;
                    mCallbackHelper.notifyCalled();
                }

                @Override
                public void onQueryFailed() {
                    mQueryFailed = true;
                    mCallbackHelper.notifyCalled();
                }
            };

    private List<ViewType> mViewTypeList;
    private boolean mQueryFailed;
    private CallbackHelper mCallbackHelper = new CallbackHelper();

    public IViewTypeCallback getBinder() {
        return mBinder;
    }

    public List<ViewType> getViewTypes() {
        return mViewTypeList;
    }

    public boolean isQueryFailed() {
        return mQueryFailed;
    }

    public void waitForCallbackInvoked() throws Exception {
        mCallbackHelper.waitForCallback(0);
    }
}
