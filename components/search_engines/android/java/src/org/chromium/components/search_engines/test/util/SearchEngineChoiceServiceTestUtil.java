// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.search_engines.test.util;

import static org.mockito.Mockito.doReturn;

import android.os.Handler;
import android.os.Looper;

import org.jni_zero.CalledByNative;
import org.mockito.Mockito;

import org.chromium.base.Promise;
import org.chromium.base.test.util.LooperUtils;
import org.chromium.components.search_engines.SearchEngineChoiceService;
import org.chromium.components.search_engines.SearchEngineCountryDelegate;

import java.lang.reflect.InvocationTargetException;
import java.util.concurrent.atomic.AtomicBoolean;

final class SearchEngineChoiceServiceTestUtil {
    private final SearchEngineCountryDelegate mMockDelegate;
    private final Promise<String> mDeviceCountry = new Promise<>();

    /** Stubs {@link SearchEngineChoiceService} for native tests. */
    @CalledByNative
    public SearchEngineChoiceServiceTestUtil() {
        mMockDelegate = Mockito.mock(SearchEngineCountryDelegate.class);
        doReturn(mDeviceCountry).when(mMockDelegate).getDeviceCountry();
        SearchEngineChoiceService.setInstanceForTests(new SearchEngineChoiceService(mMockDelegate));
    }

    /** Restores the global state after the test completes. */
    @CalledByNative
    public void destroy() {
        SearchEngineChoiceService.setInstanceForTests(null);
    }

    /**
     * Fulfills the promise returned by `SearchEngineCountryDelegate` to simulate a response from
     * the Play API.
     *
     * @param deviceCountry the result of the device country request.
     */
    @CalledByNative
    public void returnDeviceCountry(String deviceCountry) {
        mDeviceCountry.fulfill(deviceCountry);
        // `Promise` posts callback tasks on Android Looper which is not integrated with native
        // RunLoop in NativeTest. Run these tasks synchronously now.
        // TODO(crbug.com/40723709): remove this hack once Promise uses PostTask.
        runLooperTasks();
    }

    /**
     * Runs all tasks that are currently posted on the {@link Looper}'s message queue on the current
     * thread.
     */
    private static void runLooperTasks() {
        AtomicBoolean called = new AtomicBoolean(false);
        new Handler(Looper.myLooper())
                .post(
                        () -> {
                            called.set(true);
                        });

        do {
            try {
                LooperUtils.runSingleNestedLooperTask();
            } catch (IllegalArgumentException
                    | IllegalAccessException
                    | SecurityException
                    | InvocationTargetException e) {
                throw new RuntimeException(e);
            }
        } while (!called.get());
    }
}
