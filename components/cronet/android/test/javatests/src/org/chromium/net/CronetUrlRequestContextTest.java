// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net;

import static com.google.common.truth.Truth.assertThat;
import static com.google.common.truth.TruthJUnit.assume;

import static org.junit.Assert.assertThrows;

import static org.chromium.net.CronetEngine.Builder.HTTP_CACHE_IN_MEMORY;
import static org.chromium.net.CronetTestRule.getTestStorage;
import static org.chromium.net.truth.UrlResponseInfoSubject.assertThat;

import android.net.Network;
import android.os.Bundle;
import android.os.ConditionVariable;
import android.os.Handler;
import android.os.Looper;
import android.os.Process;

import androidx.test.ext.junit.runners.AndroidJUnit4;
import androidx.test.filters.SmallTest;

import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;
import org.json.JSONObject;
import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.Log;
import org.chromium.base.PathUtils;
import org.chromium.base.test.util.DoNotBatch;
import org.chromium.net.CronetTestRule.CronetImplementation;
import org.chromium.net.CronetTestRule.DisableAutomaticNetLog;
import org.chromium.net.CronetTestRule.IgnoreFor;
import org.chromium.net.CronetTestRule.RequiresMinApi;
import org.chromium.net.NetworkChangeNotifierAutoDetect.ConnectivityManagerDelegate;
import org.chromium.net.TestUrlRequestCallback.ResponseStep;
import org.chromium.net.httpflags.BaseFeature;
import org.chromium.net.httpflags.FlagValue;
import org.chromium.net.httpflags.Flags;
import org.chromium.net.impl.CronetExceptionImpl;
import org.chromium.net.impl.CronetLibraryLoader;
import org.chromium.net.impl.CronetManifest;
import org.chromium.net.impl.CronetManifestInterceptor;
import org.chromium.net.impl.CronetUrlRequestContext;
import org.chromium.net.impl.ImplVersion;
import org.chromium.net.impl.NativeCronetEngineBuilderImpl;
import org.chromium.net.impl.NetworkExceptionImpl;

import java.io.BufferedReader;
import java.io.File;
import java.io.FileReader;
import java.net.URL;
import java.util.Arrays;
import java.util.UUID;
import java.util.concurrent.Callable;
import java.util.concurrent.Executor;
import java.util.concurrent.FutureTask;
import java.util.concurrent.atomic.AtomicReference;

/** Test CronetEngine. */
@DoNotBatch(reason = "crbug/1459563")
@RunWith(AndroidJUnit4.class)
@JNINamespace("cronet")
public class CronetUrlRequestContextTest {
    @Rule public final CronetTestRule mTestRule = CronetTestRule.withManualEngineStartup();

    private static final String TAG = "CronetUrlReqCtxTest";
    // URLs used for tests.
    private static final String MOCK_CRONET_TEST_FAILED_URL = "http://mock.failed.request/-2";
    private static final String MOCK_CRONET_TEST_SUCCESS_URL = "http://mock.http/success.txt";
    private static final int MAX_FILE_SIZE = 1000000000;

    private String mUrl;
    private String mUrl404;
    private String mUrl500;

    @Before
    public void setUp() throws Exception {
        NativeTestServer.startNativeTestServer(mTestRule.getTestFramework().getContext());
        mUrl = NativeTestServer.getSuccessURL();
        mUrl404 = NativeTestServer.getNotFoundURL();
        mUrl500 = NativeTestServer.getServerErrorURL();
    }

    @After
    public void tearDown() throws Exception {
        NativeTestServer.shutdownNativeTestServer();
    }

    class RequestThread extends Thread {
        public TestUrlRequestCallback mCallback;

        final String mUrl;
        final ConditionVariable mRunBlocker;

        public RequestThread(String url, ConditionVariable runBlocker) {
            mUrl = url;
            mRunBlocker = runBlocker;
        }

        @Override
        public void run() {
            mRunBlocker.block();
            ExperimentalCronetEngine cronetEngine =
                    mTestRule
                            .getTestFramework()
                            .createNewSecondaryBuilder(mTestRule.getTestFramework().getContext())
                            .build();
            try {
                mCallback = new TestUrlRequestCallback();
                UrlRequest.Builder urlRequestBuilder =
                        cronetEngine.newUrlRequestBuilder(mUrl, mCallback, mCallback.getExecutor());
                urlRequestBuilder.build().start();
                mCallback.blockForDone();
            } finally {
                cronetEngine.shutdown();
            }
        }
    }

    /** Callback that shutdowns the request context when request has succeeded or failed. */
    static class ShutdownTestUrlRequestCallback extends TestUrlRequestCallback {
        private final CronetEngine mCronetEngine;
        private final ConditionVariable mCallbackCompletionBlock = new ConditionVariable();

        ShutdownTestUrlRequestCallback(CronetEngine cronetEngine) {
            mCronetEngine = cronetEngine;
        }

        @Override
        public void onSucceeded(UrlRequest request, UrlResponseInfo info) {
            super.onSucceeded(request, info);
            mCronetEngine.shutdown();
            mCallbackCompletionBlock.open();
        }

        @Override
        public void onFailed(UrlRequest request, UrlResponseInfo info, CronetException error) {
            super.onFailed(request, info, error);
            mCronetEngine.shutdown();
            mCallbackCompletionBlock.open();
        }

        // Wait for request completion callback.
        void blockForCallbackToComplete() {
            mCallbackCompletionBlock.block();
        }
    }

    private void setReadHttpFlagsInManifest(boolean value) {
        Bundle metaData = new Bundle();
        metaData.putBoolean(CronetManifest.READ_HTTP_FLAGS_META_DATA_KEY, value);
        mTestRule.getTestFramework().interceptContext(new CronetManifestInterceptor(metaData));
    }

    private void setLogFlag(String marker, String appId, String minVersion) {
        FlagValue.ConstrainedValue.Builder constrainedValueBuilder =
                FlagValue.ConstrainedValue.newBuilder()
                        .setStringValue("Test log flag value " + marker);
        if (appId != null) {
            constrainedValueBuilder.setAppId(appId);
        }
        if (minVersion != null) {
            constrainedValueBuilder.setMinVersion(minVersion);
        }
        mTestRule
                .getTestFramework()
                .setHttpFlags(
                        Flags.newBuilder()
                                .putFlags(
                                        CronetLibraryLoader.LOG_FLAG_NAME,
                                        FlagValue.newBuilder()
                                                .addConstrainedValues(constrainedValueBuilder)
                                                .build())
                                .build());
    }

    private void runOneRequest() {
        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        CronetEngine cronetEngine = mTestRule.getTestFramework().startEngine();
        UrlRequest.Builder urlRequestBuilder =
                cronetEngine.newUrlRequestBuilder(mUrl, callback, callback.getExecutor());
        urlRequestBuilder.build().start();
        callback.blockForDone();
    }

    private void runRequestWhileExpectingLog(String marker, boolean shouldBeLogged)
            throws Exception {
        try (LogcatCapture logcatSink =
                new LogcatCapture(
                        Arrays.asList(
                                Log.normalizeTag(CronetLibraryLoader.TAG + ":I"),
                                Log.normalizeTag(TAG + ":I"),
                                "chromium:I"))) {
            // Use the engine at least once to ensure we do not race against Cronet initialization.
            runOneRequest();

            String stopMarker = UUID.randomUUID().toString();
            Log.i(TAG, "%s --- ENGINE STARTED ---", stopMarker);

            if (shouldBeLogged) {
                while (true) {
                    String line = logcatSink.readLine();
                    assertThat(line).doesNotContain(stopMarker);
                    if (line.contains(marker)) break;
                }
                while (!logcatSink.readLine().contains(stopMarker)) {}
            } else {
                while (true) {
                    String line = logcatSink.readLine();
                    assertThat(line).doesNotContain(marker);
                    if (line.contains(stopMarker)) break;
                }
            }
        }
    }

    @Test
    @SmallTest
    @IgnoreFor(
            implementations = {CronetImplementation.FALLBACK, CronetImplementation.AOSP_PLATFORM},
            reason =
                    "HTTP flags are only supported on native Cronet for now. "
                            + "crbug.com/1495401: Emulator image does not have HttpFlags code yet")
    public void testHttpFlagsAreLoaded() throws Exception {
        setReadHttpFlagsInManifest(true);
        String marker = UUID.randomUUID().toString();
        setLogFlag(marker, /* appId= */ null, /* minVersion= */ null);
        runRequestWhileExpectingLog(marker, /* shouldBeLogged= */ true);
    }

    @Test
    @SmallTest
    @IgnoreFor(
            implementations = {CronetImplementation.FALLBACK, CronetImplementation.AOSP_PLATFORM},
            reason =
                    "HTTP flags are only supported on native Cronet for now. "
                            + "crbug.com/1495401: Emulator image does not have HttpFlags code yet")
    public void testHttpFlagsAreNotLoadedIfDisabledInManifest() throws Exception {
        setReadHttpFlagsInManifest(false);
        String marker = UUID.randomUUID().toString();
        setLogFlag(marker, /* appId= */ null, /* minVersion= */ null);
        runRequestWhileExpectingLog(marker, /* shouldBeLogged= */ false);
    }

    @Test
    @SmallTest
    @IgnoreFor(
            implementations = {CronetImplementation.FALLBACK, CronetImplementation.AOSP_PLATFORM},
            reason =
                    "HTTP flags are only supported on native Cronet for now. "
                            + "crbug.com/1495401: Emulator image does not have HttpFlags code yet")
    public void testHttpFlagsNotAppliedIfAppIdDoesntMatch() throws Exception {
        setReadHttpFlagsInManifest(true);
        String marker = UUID.randomUUID().toString();
        setLogFlag(marker, /* appId= */ "org.chromium.fake.app.id", /* minVersion= */ null);
        runRequestWhileExpectingLog(marker, /* shouldBeLogged= */ false);
    }

    @Test
    @SmallTest
    @IgnoreFor(
            implementations = {CronetImplementation.FALLBACK, CronetImplementation.AOSP_PLATFORM},
            reason =
                    "HTTP flags are only supported on native Cronet for now. "
                            + "crbug.com/1495401: Emulator image does not have HttpFlags code yet")
    public void testHttpFlagsAppliedIfAppIdMatches() throws Exception {
        setReadHttpFlagsInManifest(true);
        String marker = UUID.randomUUID().toString();
        setLogFlag(
                marker,
                /* appId= */ mTestRule.getTestFramework().getContext().getPackageName(),
                /* minVersion= */ ImplVersion.getCronetVersion());
        runRequestWhileExpectingLog(marker, /* shouldBeLogged= */ true);
    }

    @Test
    @SmallTest
    @IgnoreFor(
            implementations = {CronetImplementation.FALLBACK, CronetImplementation.AOSP_PLATFORM},
            reason =
                    "HTTP flags are only supported on native Cronet for now. "
                            + "crbug.com/1495401: Emulator image does not have HttpFlags code yet")
    public void testHttpFlagsNotAppliedIfBelowMinVersion() throws Exception {
        setReadHttpFlagsInManifest(true);
        String marker = UUID.randomUUID().toString();
        setLogFlag(marker, /* appId= */ null, /* minVersion= */ "999999.0.0.0");
        runRequestWhileExpectingLog(marker, /* shouldBeLogged= */ false);
    }

    @Test
    @SmallTest
    @IgnoreFor(
            implementations = {CronetImplementation.FALLBACK, CronetImplementation.AOSP_PLATFORM},
            reason =
                    "HTTP flags are only supported on native Cronet for now. "
                            + "crbug.com/1495401: Emulator image does not have HttpFlags code yet")
    public void testHttpFlagsAppliedIfAtMinVersion() throws Exception {
        setReadHttpFlagsInManifest(true);
        String marker = UUID.randomUUID().toString();
        setLogFlag(marker, /* appId= */ null, /* minVersion= */ ImplVersion.getCronetVersion());
        runRequestWhileExpectingLog(marker, /* shouldBeLogged= */ true);
    }

    @Test
    @SmallTest
    @IgnoreFor(
            implementations = {CronetImplementation.FALLBACK, CronetImplementation.AOSP_PLATFORM},
            reason =
                    "HTTP flags are only supported on native Cronet for now. "
                            + "crbug.com/1495401: Emulator image does not have HttpFlags code yet")
    public void testHttpFlagsAppliedIfAboveMinVersion() throws Exception {
        setReadHttpFlagsInManifest(true);
        String marker = UUID.randomUUID().toString();
        setLogFlag(marker, /* appId= */ null, /* minVersion= */ "100.0.0.0");
        runRequestWhileExpectingLog(marker, /* shouldBeLogged= */ true);
    }

    private void setChromiumBaseFeatureLogFlag(boolean enable, String marker) {
        var flags =
                Flags.newBuilder()
                        .putFlags(
                                BaseFeature.FLAG_PREFIX + "CronetLogMe",
                                FlagValue.newBuilder()
                                        .addConstrainedValues(
                                                FlagValue.ConstrainedValue.newBuilder()
                                                        .setBoolValue(enable))
                                        .build())
                        .putFlags(
                                BaseFeature.FLAG_PREFIX
                                        + "CronetLogMe"
                                        + BaseFeature.PARAM_DELIMITER
                                        + "message",
                                FlagValue.newBuilder()
                                        .addConstrainedValues(
                                                FlagValue.ConstrainedValue.newBuilder()
                                                        .setStringValue(marker))
                                        .build())
                        .build();
        mTestRule.getTestFramework().setHttpFlags(flags);
    }

    @Test
    @SmallTest
    @IgnoreFor(
            implementations = {CronetImplementation.FALLBACK, CronetImplementation.AOSP_PLATFORM},
            reason =
                    "HTTP flags are only supported on native Cronet for now. "
                            + "crbug.com/1495401: Emulator image does not have HttpFlags code yet")
    public void testBaseFeatureFlagsOverridesEnabled() throws Exception {
        setReadHttpFlagsInManifest(true);
        String marker = UUID.randomUUID().toString();
        setChromiumBaseFeatureLogFlag(true, marker);
        runRequestWhileExpectingLog(marker, /* shouldBeLogged= */ true);
    }

    @Test
    @SmallTest
    @IgnoreFor(
            implementations = {CronetImplementation.FALLBACK},
            reason = "HTTP flags are only supported on native Cronet for now")
    public void testBaseFeatureFlagsOverridesDisabled() throws Exception {
        setReadHttpFlagsInManifest(true);
        String marker = UUID.randomUUID().toString();
        setChromiumBaseFeatureLogFlag(false, marker);
        runRequestWhileExpectingLog(marker, /* shouldBeLogged= */ false);
    }

    @Test
    @SmallTest
    @SuppressWarnings("deprecation")
    public void testConfigUserAgent() throws Exception {
        String userAgentName = "User-Agent";
        String userAgentValue = "User-Agent-Value";

        mTestRule
                .getTestFramework()
                .applyEngineBuilderPatch((builder) -> builder.setUserAgent(userAgentValue));

        CronetEngine cronetEngine = mTestRule.getTestFramework().startEngine();
        NativeTestServer.shutdownNativeTestServer(); // startNativeTestServer returns false if it's
        // already running
        assertThat(
                        NativeTestServer.startNativeTestServer(
                                mTestRule.getTestFramework().getContext()))
                .isTrue();
        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        UrlRequest.Builder urlRequestBuilder =
                cronetEngine.newUrlRequestBuilder(
                        NativeTestServer.getEchoHeaderURL(userAgentName),
                        callback,
                        callback.getExecutor());
        urlRequestBuilder.build().start();
        callback.blockForDone();
        assertThat(callback.mResponseAsString).isEqualTo(userAgentValue);
    }

    @Test
    @SmallTest
    @IgnoreFor(
            implementations = {CronetImplementation.FALLBACK},
            reason = "Fallback implementation does not check for outstanding requests")
    public void testShutdown() throws Exception {
        CronetEngine cronetEngine = mTestRule.getTestFramework().startEngine();
        ShutdownTestUrlRequestCallback callback = new ShutdownTestUrlRequestCallback(cronetEngine);
        // Block callback when response starts to verify that shutdown fails
        // if there are active requests.
        callback.setAutoAdvance(false);
        UrlRequest.Builder urlRequestBuilder =
                cronetEngine.newUrlRequestBuilder(mUrl, callback, callback.getExecutor());
        UrlRequest urlRequest = urlRequestBuilder.build();
        urlRequest.start();

        Exception e = assertThrows(Exception.class, cronetEngine::shutdown);
        assertThat(e).hasMessageThat().matches("Cannot shutdown with (running|active) requests.");

        callback.waitForNextStep();
        assertThat(callback.mResponseStep).isEqualTo(ResponseStep.ON_RESPONSE_STARTED);

        e = assertThrows(Exception.class, cronetEngine::shutdown);
        assertThat(e).hasMessageThat().matches("Cannot shutdown with (running|active) requests.");

        callback.startNextRead(urlRequest);

        callback.waitForNextStep();

        assertThat(callback.mResponseStep).isEqualTo(ResponseStep.ON_READ_COMPLETED);
        e = assertThrows(Exception.class, cronetEngine::shutdown);
        assertThat(e).hasMessageThat().matches("Cannot shutdown with (running|active) requests.");

        // May not have read all the data, in theory. Just enable auto-advance
        // and finish the request.
        callback.setAutoAdvance(true);
        callback.startNextRead(urlRequest);
        callback.blockForDone();
        callback.blockForCallbackToComplete();
        callback.shutdownExecutor();
    }

    @Test
    @SmallTest
    @IgnoreFor(
            implementations = {CronetImplementation.FALLBACK, CronetImplementation.AOSP_PLATFORM},
            reason = "Tests native implementation internals")
    public void testShutdownDuringInit() throws Exception {
        final ConditionVariable block = new ConditionVariable(false);

        // Post a task to main thread to block until shutdown is called to test
        // scenario when shutdown is called right after construction before
        // context is fully initialized on the main thread.
        Runnable blockingTask =
                new Runnable() {
                    @Override
                    public void run() {
                        block.block();
                    }
                };
        // Ensure that test is not running on the main thread.
        assertThat(Looper.getMainLooper()).isNotEqualTo(Looper.myLooper());
        new Handler(Looper.getMainLooper()).post(blockingTask);

        // Create new request context, but its initialization on the main thread
        // will be stuck behind blockingTask.
        CronetUrlRequestContext cronetEngine =
                (CronetUrlRequestContext)
                        mTestRule
                                .getTestFramework()
                                .createNewSecondaryBuilder(
                                        mTestRule.getTestFramework().getContext())
                                .build();
        // Unblock the main thread, so context gets initialized and shutdown on
        // it.
        block.open();
        // Shutdown will wait for init to complete on main thread.
        cronetEngine.shutdown();
        // Verify that context is shutdown.
        Exception e = assertThrows(Exception.class, cronetEngine::getUrlRequestContextAdapter);
        assertThat(e).hasMessageThat().isEqualTo("Engine is shut down.");
    }

    @Test
    @SmallTest
    @IgnoreFor(
            implementations = {CronetImplementation.FALLBACK, CronetImplementation.AOSP_PLATFORM},
            reason = "Tests native implementation internals")
    public void testInitAndShutdownOnMainThread() throws Exception {
        final ConditionVariable block = new ConditionVariable(false);

        // Post a task to main thread to init and shutdown on the main thread.
        Runnable blockingTask =
                () -> {
                    // Create new request context, loading the library.
                    final CronetUrlRequestContext cronetEngine =
                            (CronetUrlRequestContext)
                                    mTestRule
                                            .getTestFramework()
                                            .createNewSecondaryBuilder(
                                                    mTestRule.getTestFramework().getContext())
                                            .build();
                    // Shutdown right after init.
                    cronetEngine.shutdown();
                    // Verify that context is shutdown.
                    Exception e =
                            assertThrows(
                                    Exception.class, cronetEngine::getUrlRequestContextAdapter);
                    assertThat(e).hasMessageThat().isEqualTo("Engine is shut down.");
                    block.open();
                };
        new Handler(Looper.getMainLooper()).post(blockingTask);
        // Wait for shutdown to complete on main thread.
        block.block();
    }

    @Test
    @SmallTest
    @IgnoreFor(
            implementations = {CronetImplementation.FALLBACK},
            reason = "JavaCronetEngine doesn't support throwing on repeat shutdown()")
    public void testMultipleShutdown() throws Exception {
        CronetEngine cronetEngine = mTestRule.getTestFramework().startEngine();
        cronetEngine.shutdown();
        Exception e = assertThrows(Exception.class, cronetEngine::shutdown);
        assertThat(e).hasMessageThat().isEqualTo("Engine is shut down.");
    }

    @Test
    @SmallTest
    public void testShutdownAfterError() throws Exception {
        CronetEngine cronetEngine = mTestRule.getTestFramework().startEngine();
        ShutdownTestUrlRequestCallback callback = new ShutdownTestUrlRequestCallback(cronetEngine);
        UrlRequest.Builder urlRequestBuilder =
                cronetEngine.newUrlRequestBuilder(
                        MOCK_CRONET_TEST_FAILED_URL, callback, callback.getExecutor());
        urlRequestBuilder.build().start();
        callback.blockForDone();
        assertThat(callback.mOnErrorCalled).isTrue();
        callback.blockForCallbackToComplete();
        callback.shutdownExecutor();
    }

    @Test
    @SmallTest
    @IgnoreFor(
            implementations = {CronetImplementation.FALLBACK},
            reason = "JavaCronetEngine doesn't support throwing on shutdown()")
    public void testShutdownAfterCancel() throws Exception {
        CronetEngine cronetEngine = mTestRule.getTestFramework().startEngine();
        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        // Block callback when response starts to verify that shutdown fails
        // if there are active requests.
        callback.setAutoAdvance(false);
        UrlRequest.Builder urlRequestBuilder =
                cronetEngine.newUrlRequestBuilder(mUrl, callback, callback.getExecutor());
        UrlRequest urlRequest = urlRequestBuilder.build();
        urlRequest.start();

        Exception e = assertThrows(Exception.class, cronetEngine::shutdown);
        assertThat(e).hasMessageThat().matches("Cannot shutdown with (running|active) requests.");

        callback.waitForNextStep();
        assertThat(callback.mResponseStep).isEqualTo(ResponseStep.ON_RESPONSE_STARTED);
        urlRequest.cancel();
    }

    @Test
    @SmallTest
    @IgnoreFor(
            implementations = {CronetImplementation.FALLBACK, CronetImplementation.AOSP_PLATFORM},
            reason = "Tests native implementation internals")
    public void testNetworkBoundContextLifetime() throws Exception {
        ExperimentalCronetEngine cronetEngine = mTestRule.getTestFramework().startEngine();
        ConnectivityManagerDelegate delegate =
                new ConnectivityManagerDelegate(mTestRule.getTestFramework().getContext());
        Network defaultNetwork = delegate.getDefaultNetwork();
        assume().that(defaultNetwork).isNotNull();

        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        // Allows to check the underlying network-bound context state while the request is in
        // progress.
        callback.setAutoAdvance(false);

        ExperimentalUrlRequest.Builder urlRequestBuilder =
                cronetEngine.newUrlRequestBuilder(mUrl, callback, callback.getExecutor());
        urlRequestBuilder.bindToNetwork(defaultNetwork.getNetworkHandle());
        UrlRequest urlRequest = urlRequestBuilder.build();
        assertThat(ApiHelper.doesContextExistForNetwork(cronetEngine, defaultNetwork)).isFalse();
        urlRequest.start();
        assertThat(ApiHelper.doesContextExistForNetwork(cronetEngine, defaultNetwork)).isTrue();

        // Resume callback execution.
        callback.waitForNextStep();
        assertThat(callback.mResponseStep).isEqualTo(ResponseStep.ON_RESPONSE_STARTED);
        callback.setAutoAdvance(true);
        callback.startNextRead(urlRequest);
        callback.blockForDone();
        assertThat(callback.mError).isNull();

        // The default network should still be active, hence the underlying network-bound context
        // should still be there.
        assertThat(ApiHelper.doesContextExistForNetwork(cronetEngine, defaultNetwork)).isTrue();

        // Fake disconnect event for the default network, this should destroy the underlying
        // network-bound context.
        FutureTask<Void> task =
                new FutureTask<Void>(
                        new Callable<Void>() {
                            @Override
                            public Void call() {
                                NetworkChangeNotifier.fakeNetworkDisconnected(
                                        defaultNetwork.getNetworkHandle());
                                return null;
                            }
                        });
        CronetLibraryLoader.postToInitThread(task);
        task.get();
        assertThat(ApiHelper.doesContextExistForNetwork(cronetEngine, defaultNetwork)).isFalse();
    }

    @Test
    @SmallTest
    @IgnoreFor(
            implementations = {CronetImplementation.FALLBACK, CronetImplementation.AOSP_PLATFORM},
            reason = "Tests native implementation internals")
    public void testNetworkBoundRequestCancel() throws Exception {
        // Upon a network disconnection, NCN posts a tasks onto the network thread that calls
        // CronetContext::NetworkTasks::OnNetworkDisconnected.
        // Calling urlRequest.cancel() also, after some hoops, ends up in a posted tasks onto the
        // network thread that calls CronetURLRequest::NetworkTasks::Destroy.
        // Depending on their implementation this can lead to UAF, this test is here to prevent that
        // from being introduced in the future.
        ExperimentalCronetEngine cronetEngine = mTestRule.getTestFramework().startEngine();
        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        callback.setAutoAdvance(false);
        ExperimentalUrlRequest.Builder urlRequestBuilder =
                cronetEngine.newUrlRequestBuilder(mUrl, callback, callback.getExecutor());
        ConnectivityManagerDelegate delegate =
                new ConnectivityManagerDelegate(mTestRule.getTestFramework().getContext());
        Network defaultNetwork = delegate.getDefaultNetwork();
        assume().that(defaultNetwork).isNotNull();

        urlRequestBuilder.bindToNetwork(defaultNetwork.getNetworkHandle());
        UrlRequest urlRequest = urlRequestBuilder.build();

        assertThat(ApiHelper.doesContextExistForNetwork(cronetEngine, defaultNetwork)).isFalse();
        urlRequest.start();
        assertThat(ApiHelper.doesContextExistForNetwork(cronetEngine, defaultNetwork)).isTrue();

        callback.waitForNextStep();
        assertThat(callback.mResponseStep).isEqualTo(ResponseStep.ON_RESPONSE_STARTED);
        assertThat(ApiHelper.doesContextExistForNetwork(cronetEngine, defaultNetwork)).isTrue();
        // Cronet registers for NCN notifications on the init thread (see
        // CronetLibraryLoader#ensureInitializedOnInitThread), hence we need to trigger fake
        // notifications from there.
        CronetLibraryLoader.postToInitThread(
                new Runnable() {
                    @Override
                    public void run() {
                        NetworkChangeNotifier.fakeNetworkDisconnected(
                                defaultNetwork.getNetworkHandle());
                        // Queue cancel after disconnect event.
                        urlRequest.cancel();
                    }
                });
        // Wait until the cancel call propagates (this would block undefinitely without that since
        // we previously set auto advance to false).
        callback.blockForDone();
        // mError should be null due to urlRequest.cancel().
        assertThat(callback.mError).isNull();
        // urlRequest.cancel(); should destroy the underlying network bound context.
        assertThat(ApiHelper.doesContextExistForNetwork(cronetEngine, defaultNetwork)).isFalse();
    }

    @Test
    public void testBindToInvalidNetworkFails() {
        ExperimentalCronetEngine cronetEngine = mTestRule.getTestFramework().startEngine();
        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        if (mTestRule.implementationUnderTest() == CronetImplementation.AOSP_PLATFORM) {
            // HttpEngine#bindToNetwork requires an android.net.Network object. So, in this case, it
            // will be the wrapper layer that will fail to translate that to a Network, not
            // something in net's code. Hence, the failure will manifest itself at bind time, not at
            // request execution time.
            // Note: this will never happen in prod, as translation failure can only happen if we're
            // given a fake networkHandle.
            assertThrows(
                    IllegalArgumentException.class,
                    () -> cronetEngine.bindToNetwork(-150 /* invalid network handle */));
            return;
        }

        cronetEngine.bindToNetwork(-150 /* invalid network handle */);
        ExperimentalUrlRequest.Builder builder =
                cronetEngine.newUrlRequestBuilder(mUrl, callback, callback.getExecutor());
        builder.build().start();
        callback.blockForDone();

        assertThat(callback.mError).isNotNull();
        if (mTestRule.implementationUnderTest() == CronetImplementation.FALLBACK) {
            assertThat(callback.mError).isInstanceOf(CronetExceptionImpl.class);
            assertThat(callback.mError).hasCauseThat().isInstanceOf(NetworkExceptionImpl.class);
        } else {
            assertThat(callback.mError).isInstanceOf(NetworkExceptionImpl.class);
        }
    }

    @Test
    public void testBindToDefaultNetworkSucceeds() {
        ConnectivityManagerDelegate delegate =
                new ConnectivityManagerDelegate(mTestRule.getTestFramework().getContext());
        Network defaultNetwork = delegate.getDefaultNetwork();
        assume().that(defaultNetwork).isNotNull();

        ExperimentalCronetEngine cronetEngine = mTestRule.getTestFramework().startEngine();
        cronetEngine.bindToNetwork(defaultNetwork.getNetworkHandle());
        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        ExperimentalUrlRequest.Builder builder =
                cronetEngine.newUrlRequestBuilder(mUrl, callback, callback.getExecutor());
        builder.build().start();
        callback.blockForDone();

        assertThat(callback.getResponseInfoWithChecks()).hasHttpStatusCodeThat().isEqualTo(200);
    }

    @Test
    @SmallTest
    @IgnoreFor(
            implementations = {CronetImplementation.FALLBACK, CronetImplementation.AOSP_PLATFORM},
            reason = "NetLog is supported only by the native implementation")
    @DisableAutomaticNetLog(reason = "Test is targeting NetLog")
    public void testNetLog() throws Exception {
        File directory = new File(PathUtils.getDataDirectory());
        File file = File.createTempFile("cronet", "json", directory);
        CronetEngine cronetEngine = mTestRule.getTestFramework().startEngine();
        // Start NetLog immediately after the request context is created to make
        // sure that the call won't crash the app even when the native request
        // context is not fully initialized. See crbug.com/470196.
        cronetEngine.startNetLogToFile(file.getPath(), false);

        // Start a request.
        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        UrlRequest.Builder urlRequestBuilder =
                cronetEngine.newUrlRequestBuilder(mUrl, callback, callback.getExecutor());
        urlRequestBuilder.build().start();
        callback.blockForDone();
        cronetEngine.stopNetLog();
        assertThat(file.exists()).isTrue();
        assertThat(file.length()).isNotEqualTo(0);
        assertThat(hasBytesInNetLog(file)).isFalse();
        assertThat(file.delete()).isTrue();
        assertThat(file.exists()).isFalse();
    }

    @Test
    @SmallTest
    @IgnoreFor(
            implementations = {CronetImplementation.FALLBACK, CronetImplementation.AOSP_PLATFORM},
            reason = "NetLog is supported only by the native implementation")
    @DisableAutomaticNetLog(reason = "Test is targeting NetLog")
    public void testBoundedFileNetLog() throws Exception {
        File directory = new File(PathUtils.getDataDirectory());
        File netLogDir = new File(directory, "NetLog" + System.currentTimeMillis());
        assertThat(netLogDir.exists()).isFalse();
        assertThat(netLogDir.mkdir()).isTrue();
        File logFile = new File(netLogDir, "netlog.json");
        ExperimentalCronetEngine cronetEngine = mTestRule.getTestFramework().startEngine();
        // Start NetLog immediately after the request context is created to make
        // sure that the call won't crash the app even when the native request
        // context is not fully initialized. See crbug.com/470196.
        cronetEngine.startNetLogToDisk(netLogDir.getPath(), false, MAX_FILE_SIZE);

        // Start a request.
        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        UrlRequest.Builder urlRequestBuilder =
                cronetEngine.newUrlRequestBuilder(mUrl, callback, callback.getExecutor());
        urlRequestBuilder.build().start();
        callback.blockForDone();
        cronetEngine.stopNetLog();
        assertThat(logFile.exists()).isTrue();
        assertThat(logFile.length()).isNotEqualTo(0);
        assertThat(hasBytesInNetLog(logFile)).isFalse();
        FileUtils.recursivelyDeleteFile(netLogDir);
        assertThat(netLogDir.exists()).isFalse();
    }

    @Test
    @SmallTest
    @IgnoreFor(
            implementations = {CronetImplementation.FALLBACK, CronetImplementation.AOSP_PLATFORM},
            reason = "NetLog is supported only by the native implementation")
    @DisableAutomaticNetLog(reason = "Test is targeting NetLog")
    // Tests that if stopNetLog is not explicitly called, CronetEngine.shutdown()
    // will take care of it. crbug.com/623701.
    public void testNoStopNetLog() throws Exception {
        File directory = new File(PathUtils.getDataDirectory());
        File file = File.createTempFile("cronet", "json", directory);
        ExperimentalCronetEngine cronetEngine = mTestRule.getTestFramework().startEngine();
        cronetEngine.startNetLogToFile(file.getPath(), false);

        // Start a request.
        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        UrlRequest.Builder urlRequestBuilder =
                cronetEngine.newUrlRequestBuilder(mUrl, callback, callback.getExecutor());
        urlRequestBuilder.build().start();
        callback.blockForDone();
        // Shut down the engine without calling stopNetLog.
        cronetEngine.shutdown();
        assertThat(file.exists()).isTrue();
        assertThat(file.length()).isNotEqualTo(0);
        assertThat(hasBytesInNetLog(file)).isFalse();
        assertThat(file.delete()).isTrue();
        assertThat(file.exists()).isFalse();
    }

    @Test
    @SmallTest
    @IgnoreFor(
            implementations = {CronetImplementation.FALLBACK, CronetImplementation.AOSP_PLATFORM},
            reason = "NetLog is supported only by the native implementation")
    @DisableAutomaticNetLog(reason = "Test is targeting NetLog")
    // Tests that if stopNetLog is not explicitly called, CronetEngine.shutdown()
    // will take care of it. crbug.com/623701.
    public void testNoStopBoundedFileNetLog() throws Exception {
        File directory = new File(PathUtils.getDataDirectory());
        File netLogDir = new File(directory, "NetLog" + System.currentTimeMillis());
        assertThat(netLogDir.exists()).isFalse();
        assertThat(netLogDir.mkdir()).isTrue();
        File logFile = new File(netLogDir, "netlog.json");
        ExperimentalCronetEngine cronetEngine = mTestRule.getTestFramework().startEngine();
        cronetEngine.startNetLogToDisk(netLogDir.getPath(), false, MAX_FILE_SIZE);

        // Start a request.
        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        UrlRequest.Builder urlRequestBuilder =
                cronetEngine.newUrlRequestBuilder(mUrl, callback, callback.getExecutor());
        urlRequestBuilder.build().start();
        callback.blockForDone();
        // Shut down the engine without calling stopNetLog.
        cronetEngine.shutdown();
        assertThat(logFile.exists()).isTrue();
        assertThat(logFile.length()).isNotEqualTo(0);

        FileUtils.recursivelyDeleteFile(netLogDir);
        assertThat(netLogDir.exists()).isFalse();
    }

    @Test
    @SmallTest
    @IgnoreFor(
            implementations = {CronetImplementation.AOSP_PLATFORM},
            reason = "ActiveRequestCount is not available in AOSP")
    public void testGetActiveRequestCount() throws Exception {
        CronetEngine cronetEngine = mTestRule.getTestFramework().startEngine();
        TestUrlRequestCallback callback1 = new TestUrlRequestCallback();
        TestUrlRequestCallback callback2 = new TestUrlRequestCallback();
        callback1.setAutoAdvance(false);
        callback2.setAutoAdvance(false);
        assertThat(cronetEngine.getActiveRequestCount()).isEqualTo(0);
        UrlRequest request1 =
                cronetEngine.newUrlRequestBuilder(mUrl, callback1, callback1.getExecutor()).build();
        UrlRequest request2 =
                cronetEngine.newUrlRequestBuilder(mUrl, callback2, callback2.getExecutor()).build();
        request1.start();
        request2.start();
        assertThat(cronetEngine.getActiveRequestCount()).isEqualTo(2);
        callback1.waitForNextStep();
        callback1.setAutoAdvance(true);
        callback1.startNextRead(request1);
        callback1.blockForDone();
        waitForActiveRequestCount(cronetEngine, 1);
        callback2.waitForNextStep();
        callback2.setAutoAdvance(true);
        callback2.startNextRead(request2);
        callback2.blockForDone();
        waitForActiveRequestCount(cronetEngine, 0);
    }

    @Test
    @SmallTest
    @IgnoreFor(
            implementations = {CronetImplementation.AOSP_PLATFORM},
            reason = "ActiveRequestCount is not available in AOSP")
    public void testGetActiveRequestCountOnReachingSucceeded() throws Exception {
        ExperimentalCronetEngine cronetEngine = mTestRule.getTestFramework().startEngine();
        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        callback.setAutoAdvance(false);
        callback.setBlockOnTerminalState(true);
        assertThat(cronetEngine.getActiveRequestCount()).isEqualTo(0);
        UrlRequest request =
                cronetEngine.newUrlRequestBuilder(mUrl, callback, callback.getExecutor()).build();
        request.start();
        assertThat(cronetEngine.getActiveRequestCount()).isEqualTo(1);
        callback.waitForNextStep();
        callback.startNextRead(request);
        callback.waitForNextStep();
        callback.startNextRead(request);
        callback.blockForDone();
        assertThat(cronetEngine.getActiveRequestCount()).isEqualTo(1);
        callback.setBlockOnTerminalState(false);
        waitForActiveRequestCount(cronetEngine, 0);
    }

    @Test
    @SmallTest
    @IgnoreFor(
            implementations = {CronetImplementation.AOSP_PLATFORM},
            reason = "ActiveRequestCount is not available in AOSP")
    public void testGetActiveRequestCountOnReachingCancel() throws Exception {
        ExperimentalCronetEngine cronetEngine = mTestRule.getTestFramework().startEngine();
        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        callback.setAutoAdvance(false);
        callback.setBlockOnTerminalState(true);
        assertThat(cronetEngine.getActiveRequestCount()).isEqualTo(0);
        UrlRequest request =
                cronetEngine.newUrlRequestBuilder(mUrl, callback, callback.getExecutor()).build();
        request.start();
        assertThat(cronetEngine.getActiveRequestCount()).isEqualTo(1);
        request.cancel();
        callback.blockForDone();
        assertThat(cronetEngine.getActiveRequestCount()).isEqualTo(1);
        callback.setBlockOnTerminalState(false);
        assertThat(callback.mOnCanceledCalled).isTrue();
        assertThat(callback.mResponseStep).isEqualTo(ResponseStep.ON_CANCELED);
        waitForActiveRequestCount(cronetEngine, 0);
    }

    @Test
    @SmallTest
    @IgnoreFor(
            implementations = {CronetImplementation.AOSP_PLATFORM},
            reason = "ActiveRequestCount is not available in AOSP")
    public void testGetActiveRequestCountOnReachingFail() throws Exception {
        final String badUrl = "www.unreachable-url.com";
        ExperimentalCronetEngine cronetEngine = mTestRule.getTestFramework().startEngine();
        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        callback.setAutoAdvance(false);
        callback.setBlockOnTerminalState(true);
        assertThat(cronetEngine.getActiveRequestCount()).isEqualTo(0);
        UrlRequest request =
                cronetEngine.newUrlRequestBuilder(badUrl, callback, callback.getExecutor()).build();
        request.start();
        assertThat(cronetEngine.getActiveRequestCount()).isEqualTo(1);
        callback.blockForDone();
        assertThat(cronetEngine.getActiveRequestCount()).isEqualTo(1);
        callback.setBlockOnTerminalState(false);
        assertThat(callback.mOnErrorCalled).isTrue();
        assertThat(callback.mResponseStep).isEqualTo(ResponseStep.ON_FAILED);
        waitForActiveRequestCount(cronetEngine, 0);
    }

    @Test
    @SmallTest
    @IgnoreFor(
            implementations = {CronetImplementation.FALLBACK, CronetImplementation.AOSP_PLATFORM},
            reason =
                    "crbug.com/1494901: Broken for JavaCronetEngine. "
                            + "ActiveRequestCount is not available in AOSP")
    public void testGetActiveRequestCountOnDoubleStart() throws Exception {
        ExperimentalCronetEngine cronetEngine = mTestRule.getTestFramework().startEngine();
        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        callback.setAutoAdvance(false);
        UrlRequest request =
                cronetEngine.newUrlRequestBuilder(mUrl, callback, callback.getExecutor()).build();
        assertThat(cronetEngine.getActiveRequestCount()).isEqualTo(0);
        request.start();
        assertThat(cronetEngine.getActiveRequestCount()).isEqualTo(1);
        assertThrows(Exception.class, request::start);
        assertThat(cronetEngine.getActiveRequestCount()).isEqualTo(1);
        callback.setAutoAdvance(true);
        callback.blockForDone();
        waitForActiveRequestCount(cronetEngine, 0);
    }

    @Test
    @SmallTest
    @IgnoreFor(
            implementations = {CronetImplementation.FALLBACK, CronetImplementation.AOSP_PLATFORM},
            reason =
                    "JavaCronetEngine currently never throws directly from start. "
                            + "ActiveRequestCount is not available in AOSP")
    public void testGetActiveRequestCountOnInvalidRequest() throws Exception {
        ExperimentalCronetEngine cronetEngine = mTestRule.getTestFramework().startEngine();
        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        UrlRequest request =
                cronetEngine
                        .newUrlRequestBuilder("", callback, callback.getExecutor())
                        .setHttpMethod("")
                        .build();
        assertThat(cronetEngine.getActiveRequestCount()).isEqualTo(0);
        assertThrows(Exception.class, request::start);
        assertThat(cronetEngine.getActiveRequestCount()).isEqualTo(0);
    }

    @Test
    @SmallTest
    @IgnoreFor(
            implementations = {CronetImplementation.AOSP_PLATFORM},
            reason = "ActiveRequestCount is not available in AOSP")
    public void testGetActiveRequestCountWithCancel() throws Exception {
        ExperimentalCronetEngine cronetEngine = mTestRule.getTestFramework().startEngine();
        TestUrlRequestCallback callback1 = new TestUrlRequestCallback();
        TestUrlRequestCallback callback2 = new TestUrlRequestCallback();
        callback1.setAutoAdvance(false);
        callback2.setAutoAdvance(false);
        assertThat(cronetEngine.getActiveRequestCount()).isEqualTo(0);
        UrlRequest request1 =
                cronetEngine.newUrlRequestBuilder(mUrl, callback1, callback1.getExecutor()).build();
        UrlRequest request2 =
                cronetEngine.newUrlRequestBuilder(mUrl, callback2, callback2.getExecutor()).build();
        request1.start();
        request2.start();
        assertThat(cronetEngine.getActiveRequestCount()).isEqualTo(2);
        request1.cancel();
        callback1.blockForDone();
        assertThat(callback1.mOnCanceledCalled).isTrue();
        assertThat(callback1.mResponseStep).isEqualTo(ResponseStep.ON_CANCELED);
        waitForActiveRequestCount(cronetEngine, 1);
        callback2.waitForNextStep();
        callback2.setAutoAdvance(true);
        callback2.startNextRead(request2);
        callback2.blockForDone();
        waitForActiveRequestCount(cronetEngine, 0);
    }

    @Test
    @SmallTest
    @IgnoreFor(
            implementations = {CronetImplementation.AOSP_PLATFORM},
            reason = "ActiveRequestCount is not available in AOSP")
    public void testGetActiveRequestCountWithError() throws Exception {
        final String badUrl = "www.unreachable-url.com";
        ExperimentalCronetEngine cronetEngine = mTestRule.getTestFramework().startEngine();
        TestUrlRequestCallback callback1 = new TestUrlRequestCallback();
        TestUrlRequestCallback callback2 = new TestUrlRequestCallback();
        callback1.setAutoAdvance(false);
        callback1.setBlockOnTerminalState(true);
        callback2.setAutoAdvance(false);
        assertThat(cronetEngine.getActiveRequestCount()).isEqualTo(0);
        UrlRequest request1 =
                cronetEngine
                        .newUrlRequestBuilder(badUrl, callback1, callback1.getExecutor())
                        .build();
        UrlRequest request2 =
                cronetEngine.newUrlRequestBuilder(mUrl, callback2, callback2.getExecutor()).build();
        request1.start();
        request2.start();
        assertThat(cronetEngine.getActiveRequestCount()).isEqualTo(2);
        callback1.setBlockOnTerminalState(false);
        callback1.blockForDone();
        assertThat(callback1.mOnErrorCalled).isTrue();
        assertThat(callback1.mResponseStep).isEqualTo(ResponseStep.ON_FAILED);
        waitForActiveRequestCount(cronetEngine, 1);
        callback2.waitForNextStep();
        callback2.setAutoAdvance(true);
        callback2.startNextRead(request2);
        callback2.blockForDone();
        waitForActiveRequestCount(cronetEngine, 0);
    }

    @Test
    @SmallTest
    @IgnoreFor(
            implementations = {CronetImplementation.FALLBACK, CronetImplementation.AOSP_PLATFORM},
            reason =
                    "Request finished listeners not supported in Fallback, Active request count not"
                            + " supported by AOSP.")
    public void testGetActiveRequestCountOnRequestFinishedListener() throws Exception {
        ExperimentalCronetEngine cronetEngine = mTestRule.getTestFramework().startEngine();
        TestRequestFinishedListener requestFinishedListener = new TestRequestFinishedListener();
        requestFinishedListener.blockListener();
        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        UrlRequest request =
                cronetEngine
                        .newUrlRequestBuilder(mUrl, callback, callback.getExecutor())
                        .setRequestFinishedListener(requestFinishedListener)
                        .build();
        assertThat(cronetEngine.getActiveRequestCount()).isEqualTo(0);
        request.start();
        callback.blockForDone();
        assertThat(cronetEngine.getActiveRequestCount()).isEqualTo(1);
        requestFinishedListener.blockUntilDone();
        assertThat(cronetEngine.getActiveRequestCount()).isEqualTo(1);
        requestFinishedListener.unblockListener();
        waitForActiveRequestCount(cronetEngine, 0);
    }

    @Test
    @SmallTest
    @IgnoreFor(
            implementations = {CronetImplementation.FALLBACK, CronetImplementation.AOSP_PLATFORM},
            reason =
                    "Request finished listeners not supported in Fallback, Active request count not"
                            + " supported by AOSP.")
    public void testGetActiveRequestCountOnThrowingRequestFinishedListener() throws Exception {
        ExperimentalCronetEngine cronetEngine = mTestRule.getTestFramework().startEngine();
        TestRequestFinishedListener requestFinishedListener = new TestRequestFinishedListener();
        requestFinishedListener.makeListenerThrow();
        requestFinishedListener.blockListener();
        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        UrlRequest request =
                cronetEngine
                        .newUrlRequestBuilder(mUrl, callback, callback.getExecutor())
                        .setRequestFinishedListener(requestFinishedListener)
                        .build();
        assertThat(cronetEngine.getActiveRequestCount()).isEqualTo(0);
        request.start();
        callback.blockForDone();
        assertThat(cronetEngine.getActiveRequestCount()).isEqualTo(1);
        requestFinishedListener.blockUntilDone();
        assertThat(cronetEngine.getActiveRequestCount()).isEqualTo(1);
        requestFinishedListener.unblockListener();
        waitForActiveRequestCount(cronetEngine, 0);
    }

    @Test
    @SmallTest
    @IgnoreFor(
            implementations = {CronetImplementation.FALLBACK, CronetImplementation.AOSP_PLATFORM},
            reason =
                    "Request finished listeners not supported in Fallback, Active request count not"
                            + " supported by AOSP.")
    public void testGetActiveRequestCountOnThrowingEngineRequestFinishedListener()
            throws Exception {
        ExperimentalCronetEngine cronetEngine = mTestRule.getTestFramework().startEngine();
        TestRequestFinishedListener requestFinishedListener = new TestRequestFinishedListener();
        requestFinishedListener.makeListenerThrow();
        requestFinishedListener.blockListener();
        cronetEngine.addRequestFinishedListener(requestFinishedListener);
        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        UrlRequest request =
                cronetEngine.newUrlRequestBuilder(mUrl, callback, callback.getExecutor()).build();
        assertThat(cronetEngine.getActiveRequestCount()).isEqualTo(0);
        request.start();
        callback.blockForDone();
        assertThat(cronetEngine.getActiveRequestCount()).isEqualTo(1);
        requestFinishedListener.blockUntilDone();
        assertThat(cronetEngine.getActiveRequestCount()).isEqualTo(1);
        requestFinishedListener.unblockListener();
        waitForActiveRequestCount(cronetEngine, 0);
    }

    @Test
    @SmallTest
    @IgnoreFor(
            implementations = {CronetImplementation.FALLBACK, CronetImplementation.AOSP_PLATFORM},
            reason =
                    "Request finished listeners not supported in Fallback, Active request count not"
                            + " supported by AOSP.")
    public void testGetActiveRequestCountOnEngineRequestFinishedListener() throws Exception {
        ExperimentalCronetEngine cronetEngine = mTestRule.getTestFramework().startEngine();
        TestRequestFinishedListener requestFinishedListener = new TestRequestFinishedListener();
        requestFinishedListener.blockListener();
        cronetEngine.addRequestFinishedListener(requestFinishedListener);
        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        UrlRequest request =
                cronetEngine.newUrlRequestBuilder(mUrl, callback, callback.getExecutor()).build();
        assertThat(cronetEngine.getActiveRequestCount()).isEqualTo(0);
        request.start();
        callback.blockForDone();
        assertThat(cronetEngine.getActiveRequestCount()).isEqualTo(1);
        requestFinishedListener.blockUntilDone();
        assertThat(cronetEngine.getActiveRequestCount()).isEqualTo(1);
        requestFinishedListener.unblockListener();
        waitForActiveRequestCount(cronetEngine, 0);
    }

    @Test
    @SmallTest
    @IgnoreFor(
            implementations = {CronetImplementation.FALLBACK, CronetImplementation.AOSP_PLATFORM},
            reason = "NetLog is supported only by the native implementation")
    @DisableAutomaticNetLog(reason = "Test is targeting NetLog")
    // Tests that NetLog contains events emitted by all live CronetEngines.
    public void testNetLogContainEventsFromAllLiveEngines() throws Exception {
        File directory = new File(PathUtils.getDataDirectory());
        File file1 = File.createTempFile("cronet1", "json", directory);
        File file2 = File.createTempFile("cronet2", "json", directory);
        CronetEngine cronetEngine1 =
                mTestRule
                        .getTestFramework()
                        .createNewSecondaryBuilder(mTestRule.getTestFramework().getContext())
                        .build();
        CronetEngine cronetEngine2 =
                mTestRule
                        .getTestFramework()
                        .createNewSecondaryBuilder(mTestRule.getTestFramework().getContext())
                        .build();

        cronetEngine1.startNetLogToFile(file1.getPath(), false);
        cronetEngine2.startNetLogToFile(file2.getPath(), false);

        // Warm CronetEngine and make sure both CronetUrlRequestContexts are
        // initialized before testing the logs.
        makeRequestAndCheckStatus(cronetEngine1, mUrl, 200);
        makeRequestAndCheckStatus(cronetEngine2, mUrl, 200);

        // Use cronetEngine1 to make a request to mUrl404.
        makeRequestAndCheckStatus(cronetEngine1, mUrl404, 404);

        // Use cronetEngine2 to make a request to mUrl500.
        makeRequestAndCheckStatus(cronetEngine2, mUrl500, 500);

        cronetEngine1.stopNetLog();
        cronetEngine2.stopNetLog();
        assertThat(file1.exists()).isTrue();
        assertThat(file2.exists()).isTrue();
        // Make sure both files contain the two requests made separately using
        // different engines.
        assertThat(containsStringInNetLog(file1, mUrl404)).isTrue();
        assertThat(containsStringInNetLog(file1, mUrl500)).isTrue();
        assertThat(containsStringInNetLog(file2, mUrl404)).isTrue();
        assertThat(containsStringInNetLog(file2, mUrl500)).isTrue();
        assertThat(file1.delete()).isTrue();
        assertThat(file2.delete()).isTrue();
    }

    @Test
    @SmallTest
    @IgnoreFor(
            implementations = {CronetImplementation.FALLBACK, CronetImplementation.AOSP_PLATFORM},
            reason = "NetLog is supported only by the native implementation")
    @DisableAutomaticNetLog(reason = "Test is targeting NetLog")
    // Tests that NetLog contains events emitted by all live CronetEngines.
    public void testBoundedFileNetLogContainEventsFromAllLiveEngines() throws Exception {
        File directory = new File(PathUtils.getDataDirectory());
        File netLogDir1 = new File(directory, "NetLog1" + System.currentTimeMillis());
        assertThat(netLogDir1.exists()).isFalse();
        assertThat(netLogDir1.mkdir()).isTrue();
        File netLogDir2 = new File(directory, "NetLog2" + System.currentTimeMillis());
        assertThat(netLogDir2.exists()).isFalse();
        assertThat(netLogDir2.mkdir()).isTrue();
        File logFile1 = new File(netLogDir1, "netlog.json");
        File logFile2 = new File(netLogDir2, "netlog.json");

        CronetEngine cronetEngine1 =
                mTestRule
                        .getTestFramework()
                        .createNewSecondaryBuilder(mTestRule.getTestFramework().getContext())
                        .build();
        CronetEngine cronetEngine2 =
                mTestRule
                        .getTestFramework()
                        .createNewSecondaryBuilder(mTestRule.getTestFramework().getContext())
                        .build();

        cronetEngine1.startNetLogToDisk(netLogDir1.getPath(), false, MAX_FILE_SIZE);
        cronetEngine2.startNetLogToDisk(netLogDir2.getPath(), false, MAX_FILE_SIZE);

        // Warm CronetEngine and make sure both CronetUrlRequestContexts are
        // initialized before testing the logs.
        makeRequestAndCheckStatus(cronetEngine1, mUrl, 200);
        makeRequestAndCheckStatus(cronetEngine2, mUrl, 200);

        // Use cronetEngine1 to make a request to mUrl404.
        makeRequestAndCheckStatus(cronetEngine1, mUrl404, 404);

        // Use cronetEngine2 to make a request to mUrl500.
        makeRequestAndCheckStatus(cronetEngine2, mUrl500, 500);

        cronetEngine1.stopNetLog();
        cronetEngine2.stopNetLog();

        assertThat(logFile1.exists()).isTrue();
        assertThat(logFile2.exists()).isTrue();
        assertThat(logFile1.length()).isNotEqualTo(0);
        assertThat(logFile2.length()).isNotEqualTo(0);

        // Make sure both files contain the two requests made separately using
        // different engines.
        assertThat(containsStringInNetLog(logFile1, mUrl404)).isTrue();
        assertThat(containsStringInNetLog(logFile1, mUrl500)).isTrue();
        assertThat(containsStringInNetLog(logFile2, mUrl404)).isTrue();
        assertThat(containsStringInNetLog(logFile2, mUrl500)).isTrue();

        FileUtils.recursivelyDeleteFile(netLogDir1);
        assertThat(netLogDir1.exists()).isFalse();
        FileUtils.recursivelyDeleteFile(netLogDir2);
        assertThat(netLogDir2.exists()).isFalse();
    }

    private CronetEngine createCronetEngineWithCache(int cacheType) {
        CronetEngine.Builder builder =
                mTestRule
                        .getTestFramework()
                        .createNewSecondaryBuilder(mTestRule.getTestFramework().getContext());
        if (cacheType == CronetEngine.Builder.HTTP_CACHE_DISK
                || cacheType == CronetEngine.Builder.HTTP_CACHE_DISK_NO_HTTP) {
            builder.setStoragePath(getTestStorage(mTestRule.getTestFramework().getContext()));
        }
        builder.enableHttpCache(cacheType, 100 * 1024);
        // Don't check the return value here, because startNativeTestServer() returns false when the
        // NativeTestServer is already running and this method needs to be called twice without
        // shutting down the NativeTestServer in between.
        NativeTestServer.startNativeTestServer(mTestRule.getTestFramework().getContext());
        return builder.build();
    }

    @Test
    @SmallTest
    @IgnoreFor(
            implementations = {CronetImplementation.FALLBACK},
            reason = "Fallback implementation does not have a network thread.")
    // Tests that if CronetEngine is shut down on the network thread, an appropriate exception
    // is thrown.
    public void testShutDownEngineOnNetworkThread() throws Exception {
        final CronetEngine cronetEngine =
                createCronetEngineWithCache(CronetEngine.Builder.HTTP_CACHE_DISK);
        String url = NativeTestServer.getFileURL("/cacheable.txt");
        // Make a request to a cacheable resource.
        checkRequestCaching(cronetEngine, url, false);

        final AtomicReference<Throwable> thrown = new AtomicReference<>();
        // Shut down the server.
        NativeTestServer.shutdownNativeTestServer();
        class CancelUrlRequestCallback extends TestUrlRequestCallback {
            @Override
            public void onResponseStarted(UrlRequest request, UrlResponseInfo info) {
                super.onResponseStarted(request, info);
                request.cancel();
                // Shut down CronetEngine immediately after request is destroyed.
                try {
                    cronetEngine.shutdown();
                } catch (Exception e) {
                    thrown.set(e);
                }
            }

            @Override
            public void onSucceeded(UrlRequest request, UrlResponseInfo info) {
                // onSucceeded will not happen, because the request is canceled
                // after sending first read and the executor is single threaded.
                throw new AssertionError("Unexpected");
            }

            @Override
            public void onFailed(UrlRequest request, UrlResponseInfo info, CronetException error) {
                throw new AssertionError("Unexpected");
            }
        }
        Executor directExecutor =
                new Executor() {
                    @Override
                    public void execute(Runnable command) {
                        command.run();
                    }
                };
        CancelUrlRequestCallback callback = new CancelUrlRequestCallback();
        callback.setAllowDirectExecutor(true);
        UrlRequest.Builder urlRequestBuilder =
                cronetEngine.newUrlRequestBuilder(url, callback, directExecutor);
        urlRequestBuilder.allowDirectExecutor();
        urlRequestBuilder.build().start();
        callback.blockForDone();
        assertThat(thrown.get()).isInstanceOf(RuntimeException.class);
        cronetEngine.shutdown();
    }

    @Test
    @SmallTest
    @IgnoreFor(
            implementations = {CronetImplementation.FALLBACK},
            reason = "Fallback implementation has no support for caches")
    // Tests that if CronetEngine is shut down when reading from disk cache,
    // there isn't a crash. See crbug.com/486120.
    public void testShutDownEngineWhenReadingFromDiskCache() throws Exception {
        final CronetEngine cronetEngine =
                createCronetEngineWithCache(CronetEngine.Builder.HTTP_CACHE_DISK);
        String url = NativeTestServer.getFileURL("/cacheable.txt");
        // Make a request to a cacheable resource.
        checkRequestCaching(cronetEngine, url, false);

        // Shut down the server.
        NativeTestServer.shutdownNativeTestServer();
        class CancelUrlRequestCallback extends TestUrlRequestCallback {
            @Override
            public void onResponseStarted(UrlRequest request, UrlResponseInfo info) {
                super.onResponseStarted(request, info);
                request.cancel();
                // Shut down CronetEngine immediately after request is destroyed.
                cronetEngine.shutdown();
            }

            @Override
            public void onSucceeded(UrlRequest request, UrlResponseInfo info) {
                // onSucceeded will not happen, because the request is canceled
                // after sending first read and the executor is single threaded.
                throw new RuntimeException("Unexpected");
            }

            @Override
            public void onFailed(UrlRequest request, UrlResponseInfo info, CronetException error) {
                throw new RuntimeException("Unexpected");
            }
        }
        CancelUrlRequestCallback callback = new CancelUrlRequestCallback();
        UrlRequest.Builder urlRequestBuilder =
                cronetEngine.newUrlRequestBuilder(url, callback, callback.getExecutor());
        urlRequestBuilder.build().start();
        callback.blockForDone();
        assertThat(callback.getResponseInfoWithChecks()).hasHttpStatusCodeThat().isEqualTo(200);
        assertThat(callback.getResponseInfoWithChecks()).wasCached();
        assertThat(callback.mOnCanceledCalled).isTrue();
    }

    @Test
    @SmallTest
    @IgnoreFor(
            implementations = {CronetImplementation.FALLBACK, CronetImplementation.AOSP_PLATFORM},
            reason = "NetLog is supported only by the native implementation")
    @DisableAutomaticNetLog(reason = "Test is targeting NetLog")
    public void testNetLogAfterShutdown() throws Exception {
        ExperimentalCronetEngine cronetEngine = mTestRule.getTestFramework().startEngine();
        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        UrlRequest.Builder urlRequestBuilder =
                cronetEngine.newUrlRequestBuilder(mUrl, callback, callback.getExecutor());
        urlRequestBuilder.build().start();
        callback.blockForDone();
        cronetEngine.shutdown();

        File directory = new File(PathUtils.getDataDirectory());
        File file = File.createTempFile("cronet", "json", directory);

        Exception e =
                assertThrows(
                        Exception.class,
                        () -> cronetEngine.startNetLogToFile(file.getPath(), false));
        assertThat(e).hasMessageThat().isEqualTo("Engine is shut down.");
        assertThat(hasBytesInNetLog(file)).isFalse();
        assertThat(file.delete()).isTrue();
        assertThat(file.exists()).isFalse();
    }

    @Test
    @SmallTest
    @IgnoreFor(
            implementations = {CronetImplementation.FALLBACK, CronetImplementation.AOSP_PLATFORM},
            reason = "NetLog is supported only by the native implementation")
    @DisableAutomaticNetLog(reason = "Test is targeting NetLog")
    public void testBoundedFileNetLogAfterShutdown() throws Exception {
        ExperimentalCronetEngine cronetEngine = mTestRule.getTestFramework().startEngine();
        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        UrlRequest.Builder urlRequestBuilder =
                cronetEngine.newUrlRequestBuilder(mUrl, callback, callback.getExecutor());
        urlRequestBuilder.build().start();
        callback.blockForDone();
        cronetEngine.shutdown();

        File directory = new File(PathUtils.getDataDirectory());
        File netLogDir = new File(directory, "NetLog" + System.currentTimeMillis());
        assertThat(netLogDir.exists()).isFalse();
        assertThat(netLogDir.mkdir()).isTrue();
        File logFile = new File(netLogDir, "netlog.json");
        Exception e =
                assertThrows(
                        Exception.class,
                        () ->
                                cronetEngine.startNetLogToDisk(
                                        netLogDir.getPath(), false, MAX_FILE_SIZE));
        assertThat(e).hasMessageThat().isEqualTo("Engine is shut down.");
        assertThat(logFile.exists()).isFalse();
        FileUtils.recursivelyDeleteFile(netLogDir);
        assertThat(netLogDir.exists()).isFalse();
    }

    @Test
    @SmallTest
    @IgnoreFor(
            implementations = {CronetImplementation.FALLBACK, CronetImplementation.AOSP_PLATFORM},
            reason = "NetLog is supported only by the native implementation")
    @DisableAutomaticNetLog(reason = "Test is targeting NetLog")
    public void testNetLogStartMultipleTimes() throws Exception {
        ExperimentalCronetEngine cronetEngine = mTestRule.getTestFramework().startEngine();
        File directory = new File(PathUtils.getDataDirectory());
        File file = File.createTempFile("cronet", "json", directory);
        // Start NetLog multiple times.
        cronetEngine.startNetLogToFile(file.getPath(), false);
        cronetEngine.startNetLogToFile(file.getPath(), false);
        cronetEngine.startNetLogToFile(file.getPath(), false);
        cronetEngine.startNetLogToFile(file.getPath(), false);
        // Start a request.
        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        UrlRequest.Builder urlRequestBuilder =
                cronetEngine.newUrlRequestBuilder(mUrl, callback, callback.getExecutor());
        urlRequestBuilder.build().start();
        callback.blockForDone();
        cronetEngine.stopNetLog();
        assertThat(file.exists()).isTrue();
        assertThat(file.length()).isNotEqualTo(0);
        assertThat(hasBytesInNetLog(file)).isFalse();
        assertThat(file.delete()).isTrue();
        assertThat(file.exists()).isFalse();
    }

    @Test
    @SmallTest
    @IgnoreFor(
            implementations = {CronetImplementation.FALLBACK, CronetImplementation.AOSP_PLATFORM},
            reason = "NetLog is supported only by the native implementation")
    @DisableAutomaticNetLog(reason = "Test is targeting NetLog")
    public void testBoundedFileNetLogStartMultipleTimes() throws Exception {
        ExperimentalCronetEngine cronetEngine = mTestRule.getTestFramework().startEngine();
        File directory = new File(PathUtils.getDataDirectory());
        File netLogDir = new File(directory, "NetLog" + System.currentTimeMillis());
        assertThat(netLogDir.exists()).isFalse();
        assertThat(netLogDir.mkdir()).isTrue();
        File logFile = new File(netLogDir, "netlog.json");
        // Start NetLog multiple times. This should be equivalent to starting NetLog
        // once. Each subsequent start (without calling stopNetLog) should be a no-op.
        cronetEngine.startNetLogToDisk(netLogDir.getPath(), false, MAX_FILE_SIZE);
        cronetEngine.startNetLogToDisk(netLogDir.getPath(), false, MAX_FILE_SIZE);
        cronetEngine.startNetLogToDisk(netLogDir.getPath(), false, MAX_FILE_SIZE);
        cronetEngine.startNetLogToDisk(netLogDir.getPath(), false, MAX_FILE_SIZE);
        // Start a request.
        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        UrlRequest.Builder urlRequestBuilder =
                cronetEngine.newUrlRequestBuilder(mUrl, callback, callback.getExecutor());
        urlRequestBuilder.build().start();
        callback.blockForDone();
        cronetEngine.stopNetLog();
        assertThat(logFile.exists()).isTrue();
        assertThat(logFile.length()).isNotEqualTo(0);
        assertThat(hasBytesInNetLog(logFile)).isFalse();
        FileUtils.recursivelyDeleteFile(netLogDir);
        assertThat(netLogDir.exists()).isFalse();
    }

    @Test
    @SmallTest
    @IgnoreFor(
            implementations = {CronetImplementation.FALLBACK, CronetImplementation.AOSP_PLATFORM},
            reason = "NetLog is supported only by the native implementation")
    @DisableAutomaticNetLog(reason = "Test is targeting NetLog")
    public void testNetLogStopMultipleTimes() throws Exception {
        ExperimentalCronetEngine cronetEngine = mTestRule.getTestFramework().startEngine();
        File directory = new File(PathUtils.getDataDirectory());
        File file = File.createTempFile("cronet", "json", directory);
        cronetEngine.startNetLogToFile(file.getPath(), false);
        // Start a request.
        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        UrlRequest.Builder urlRequestBuilder =
                cronetEngine.newUrlRequestBuilder(mUrl, callback, callback.getExecutor());
        urlRequestBuilder.build().start();
        callback.blockForDone();
        // Stop NetLog multiple times.
        cronetEngine.stopNetLog();
        cronetEngine.stopNetLog();
        cronetEngine.stopNetLog();
        cronetEngine.stopNetLog();
        cronetEngine.stopNetLog();
        assertThat(file.exists()).isTrue();
        assertThat(file.length()).isNotEqualTo(0);
        assertThat(hasBytesInNetLog(file)).isFalse();
        assertThat(file.delete()).isTrue();
        assertThat(file.exists()).isFalse();
    }

    @Test
    @SmallTest
    @IgnoreFor(
            implementations = {CronetImplementation.FALLBACK, CronetImplementation.AOSP_PLATFORM},
            reason = "NetLog is supported only by the native implementation")
    @DisableAutomaticNetLog(reason = "Test is targeting NetLog")
    public void testBoundedFileNetLogStopMultipleTimes() throws Exception {
        ExperimentalCronetEngine cronetEngine = mTestRule.getTestFramework().startEngine();
        File directory = new File(PathUtils.getDataDirectory());
        File netLogDir = new File(directory, "NetLog" + System.currentTimeMillis());
        assertThat(netLogDir.exists()).isFalse();
        assertThat(netLogDir.mkdir()).isTrue();
        File logFile = new File(netLogDir, "netlog.json");
        cronetEngine.startNetLogToDisk(netLogDir.getPath(), false, MAX_FILE_SIZE);
        // Start a request.
        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        UrlRequest.Builder urlRequestBuilder =
                cronetEngine.newUrlRequestBuilder(mUrl, callback, callback.getExecutor());
        urlRequestBuilder.build().start();
        callback.blockForDone();
        // Stop NetLog multiple times. This should be equivalent to stopping NetLog once.
        // Each subsequent stop (without calling startNetLogToDisk first) should be a no-op.
        cronetEngine.stopNetLog();
        cronetEngine.stopNetLog();
        cronetEngine.stopNetLog();
        cronetEngine.stopNetLog();
        cronetEngine.stopNetLog();
        assertThat(logFile.exists()).isTrue();
        assertThat(logFile.length()).isNotEqualTo(0);
        assertThat(hasBytesInNetLog(logFile)).isFalse();
        FileUtils.recursivelyDeleteFile(netLogDir);
        assertThat(netLogDir.exists()).isFalse();
    }

    @Test
    @SmallTest
    @IgnoreFor(
            implementations = {CronetImplementation.FALLBACK, CronetImplementation.AOSP_PLATFORM},
            reason = "NetLog is supported only by the native implementation")
    @DisableAutomaticNetLog(reason = "Test is targeting NetLog")
    public void testNetLogWithBytes() throws Exception {
        File directory = new File(PathUtils.getDataDirectory());
        File file = File.createTempFile("cronet", "json", directory);
        ExperimentalCronetEngine cronetEngine = mTestRule.getTestFramework().startEngine();
        // Start NetLog with logAll as true.
        cronetEngine.startNetLogToFile(file.getPath(), true);
        // Start a request.
        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        UrlRequest.Builder urlRequestBuilder =
                cronetEngine.newUrlRequestBuilder(mUrl, callback, callback.getExecutor());
        urlRequestBuilder.build().start();
        callback.blockForDone();
        cronetEngine.stopNetLog();
        assertThat(file.exists()).isTrue();
        assertThat(file.length()).isNotEqualTo(0);
        assertThat(hasBytesInNetLog(file)).isTrue();
        assertThat(file.delete()).isTrue();
        assertThat(file.exists()).isFalse();
    }

    @Test
    @SmallTest
    @IgnoreFor(
            implementations = {CronetImplementation.FALLBACK, CronetImplementation.AOSP_PLATFORM},
            reason = "NetLog is supported only by the native implementation")
    @DisableAutomaticNetLog(reason = "Test is targeting NetLog")
    public void testBoundedFileNetLogWithBytes() throws Exception {
        File directory = new File(PathUtils.getDataDirectory());
        File netLogDir = new File(directory, "NetLog" + System.currentTimeMillis());
        assertThat(netLogDir.exists()).isFalse();
        assertThat(netLogDir.mkdir()).isTrue();
        File logFile = new File(netLogDir, "netlog.json");
        ExperimentalCronetEngine cronetEngine = mTestRule.getTestFramework().startEngine();
        // Start NetLog with logAll as true.
        cronetEngine.startNetLogToDisk(netLogDir.getPath(), true, MAX_FILE_SIZE);
        // Start a request.
        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        UrlRequest.Builder urlRequestBuilder =
                cronetEngine.newUrlRequestBuilder(mUrl, callback, callback.getExecutor());
        urlRequestBuilder.build().start();
        callback.blockForDone();
        cronetEngine.stopNetLog();

        assertThat(logFile.exists()).isTrue();
        assertThat(logFile.length()).isNotEqualTo(0);
        assertThat(hasBytesInNetLog(logFile)).isTrue();
        FileUtils.recursivelyDeleteFile(netLogDir);
        assertThat(netLogDir.exists()).isFalse();
    }

    private boolean hasBytesInNetLog(File logFile) throws Exception {
        return containsStringInNetLog(logFile, "\"bytes\"");
    }

    private boolean containsStringInNetLog(File logFile, String content) throws Exception {
        BufferedReader logReader = new BufferedReader(new FileReader(logFile));
        try {
            String logLine;
            while ((logLine = logReader.readLine()) != null) {
                if (logLine.contains(content)) {
                    return true;
                }
            }
            return false;
        } finally {
            logReader.close();
        }
    }

    /**
     * Helper method to make a request to {@code url}, wait for it to complete, and check that the
     * status code is the same as {@code expectedStatusCode}.
     */
    private void makeRequestAndCheckStatus(
            CronetEngine engine, String url, int expectedStatusCode) {
        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        UrlRequest request =
                engine.newUrlRequestBuilder(url, callback, callback.getExecutor()).build();
        request.start();
        callback.blockForDone();
        assertThat(callback.getResponseInfoWithChecks())
                .hasHttpStatusCodeThat()
                .isEqualTo(expectedStatusCode);
    }

    private void checkRequestCaching(CronetEngine engine, String url, boolean expectCached) {
        checkRequestCaching(engine, url, expectCached, false);
    }

    private void checkRequestCaching(
            CronetEngine engine, String url, boolean expectCached, boolean disableCache) {
        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        UrlRequest.Builder urlRequestBuilder =
                engine.newUrlRequestBuilder(url, callback, callback.getExecutor());
        if (disableCache) {
            urlRequestBuilder.disableCache();
        }
        urlRequestBuilder.build().start();
        callback.blockForDone();
        assertThat(callback.getResponseInfoWithChecks().wasCached()).isEqualTo(expectCached);
        assertThat(callback.mResponseAsString).isEqualTo("this is a cacheable file\n");
    }

    @Test
    @SmallTest
    @IgnoreFor(
            implementations = {CronetImplementation.FALLBACK},
            reason = "No caches support for fallback implementation")
    public void testEnableHttpCacheDisabled() throws Exception {
        CronetEngine cronetEngine =
                createCronetEngineWithCache(CronetEngine.Builder.HTTP_CACHE_DISABLED);
        String url = NativeTestServer.getFileURL("/cacheable.txt");
        checkRequestCaching(cronetEngine, url, false);
        checkRequestCaching(cronetEngine, url, false);
        checkRequestCaching(cronetEngine, url, false);
        cronetEngine.shutdown();
    }

    @Test
    @SmallTest
    @IgnoreFor(
            implementations = {CronetImplementation.FALLBACK},
            reason = "No caches support for fallback implementation")
    public void testEnableHttpCacheInMemory() throws Exception {
        CronetEngine cronetEngine =
                createCronetEngineWithCache(CronetEngine.Builder.HTTP_CACHE_IN_MEMORY);
        String url = NativeTestServer.getFileURL("/cacheable.txt");
        checkRequestCaching(cronetEngine, url, false);
        checkRequestCaching(cronetEngine, url, true);
        NativeTestServer.shutdownNativeTestServer();
        checkRequestCaching(cronetEngine, url, true);
        cronetEngine.shutdown();
    }

    @Test
    @SmallTest
    @IgnoreFor(
            implementations = {CronetImplementation.FALLBACK},
            reason = "No caches support for fallback implementation")
    public void testEnableHttpCacheDisk() throws Exception {
        CronetEngine cronetEngine =
                createCronetEngineWithCache(CronetEngine.Builder.HTTP_CACHE_DISK);
        String url = NativeTestServer.getFileURL("/cacheable.txt");
        checkRequestCaching(cronetEngine, url, false);
        checkRequestCaching(cronetEngine, url, true);
        NativeTestServer.shutdownNativeTestServer();
        checkRequestCaching(cronetEngine, url, true);
        cronetEngine.shutdown();
    }

    @Test
    @SmallTest
    @IgnoreFor(
            implementations = {CronetImplementation.FALLBACK},
            reason = "No caches support for fallback implementation")
    public void testNoConcurrentDiskUsage() throws Exception {
        CronetEngine cronetEngine =
                createCronetEngineWithCache(CronetEngine.Builder.HTTP_CACHE_DISK);

        IllegalStateException e =
                assertThrows(
                        IllegalStateException.class,
                        () -> createCronetEngineWithCache(CronetEngine.Builder.HTTP_CACHE_DISK));
        assertThat(e).hasMessageThat().isEqualTo("Disk cache storage path already in use");

        String url = NativeTestServer.getFileURL("/cacheable.txt");
        checkRequestCaching(cronetEngine, url, false);
        checkRequestCaching(cronetEngine, url, true);
        NativeTestServer.shutdownNativeTestServer();
        checkRequestCaching(cronetEngine, url, true);
        cronetEngine.shutdown();
    }

    @Test
    @SmallTest
    @IgnoreFor(
            implementations = {CronetImplementation.FALLBACK},
            reason = "No caches support for fallback implementation")
    public void testEnableHttpCacheDiskNoHttp() throws Exception {
        CronetEngine cronetEngine =
                createCronetEngineWithCache(CronetEngine.Builder.HTTP_CACHE_DISK_NO_HTTP);
        String url = NativeTestServer.getFileURL("/cacheable.txt");
        checkRequestCaching(cronetEngine, url, false);
        checkRequestCaching(cronetEngine, url, false);
        checkRequestCaching(cronetEngine, url, false);

        // Make a new CronetEngine and try again to make sure the response didn't get cached on the
        // first request. See https://crbug.com/743232.
        cronetEngine.shutdown();
        cronetEngine = createCronetEngineWithCache(CronetEngine.Builder.HTTP_CACHE_DISK_NO_HTTP);
        checkRequestCaching(cronetEngine, url, false);
        checkRequestCaching(cronetEngine, url, false);
        checkRequestCaching(cronetEngine, url, false);
        cronetEngine.shutdown();
    }

    @Test
    @SmallTest
    @IgnoreFor(
            implementations = {CronetImplementation.FALLBACK},
            reason = "No caches support for fallback implementation")
    public void testDisableCache() throws Exception {
        CronetEngine cronetEngine =
                createCronetEngineWithCache(CronetEngine.Builder.HTTP_CACHE_DISK);
        String url = NativeTestServer.getFileURL("/cacheable.txt");

        // When cache is disabled, making a request does not write to the cache.
        checkRequestCaching(cronetEngine, url, false, true);
        checkRequestCaching(cronetEngine, url, false);

        // When cache is enabled, the second request is cached.
        checkRequestCaching(cronetEngine, url, false, true);
        checkRequestCaching(cronetEngine, url, true);

        // Shut down the server, next request should have a cached response.
        NativeTestServer.shutdownNativeTestServer();
        checkRequestCaching(cronetEngine, url, true);

        // Cache is disabled after server is shut down, request should fail.
        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        UrlRequest.Builder urlRequestBuilder =
                cronetEngine.newUrlRequestBuilder(url, callback, callback.getExecutor());
        urlRequestBuilder.disableCache();
        urlRequestBuilder.build().start();
        callback.blockForDone();
        assertThat(callback.mError)
                .hasMessageThat()
                .contains("Exception in CronetUrlRequest: net::ERR_CONNECTION_REFUSED");
        cronetEngine.shutdown();
    }

    @Test
    @SmallTest
    @IgnoreFor(
            implementations = {CronetImplementation.FALLBACK},
            reason = "No caches support for fallback implementation")
    public void testEnableHttpCacheDiskNewEngine() throws Exception {
        CronetEngine cronetEngine =
                createCronetEngineWithCache(CronetEngine.Builder.HTTP_CACHE_DISK);
        String url = NativeTestServer.getFileURL("/cacheable.txt");
        checkRequestCaching(cronetEngine, url, false);
        checkRequestCaching(cronetEngine, url, true);
        NativeTestServer.shutdownNativeTestServer();
        checkRequestCaching(cronetEngine, url, true);

        // Shutdown original context and create another that uses the same cache.
        cronetEngine.shutdown();
        cronetEngine =
                mTestRule
                        .getTestFramework()
                        .enableDiskCache(
                                mTestRule
                                        .getTestFramework()
                                        .createNewSecondaryBuilder(
                                                mTestRule.getTestFramework().getContext()))
                        .build();
        checkRequestCaching(cronetEngine, url, true);
        cronetEngine.shutdown();
    }

    @Test
    @SmallTest
    public void testInitEngineAndStartRequest() {
        // Immediately make a request after initializing the engine.
        ExperimentalCronetEngine cronetEngine = mTestRule.getTestFramework().startEngine();
        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        UrlRequest.Builder urlRequestBuilder =
                cronetEngine.newUrlRequestBuilder(mUrl, callback, callback.getExecutor());
        urlRequestBuilder.build().start();
        callback.blockForDone();
        assertThat(callback.getResponseInfoWithChecks()).hasHttpStatusCodeThat().isEqualTo(200);
    }

    @Test
    @SmallTest
    public void testInitEngineStartTwoRequests() throws Exception {
        // Make two requests after initializing the context.
        ExperimentalCronetEngine cronetEngine = mTestRule.getTestFramework().startEngine();
        int[] statusCodes = {0, 0};
        String[] urls = {mUrl, mUrl404};
        for (int i = 0; i < 2; i++) {
            TestUrlRequestCallback callback = new TestUrlRequestCallback();
            UrlRequest.Builder urlRequestBuilder =
                    cronetEngine.newUrlRequestBuilder(urls[i], callback, callback.getExecutor());
            urlRequestBuilder.build().start();
            callback.blockForDone();
            statusCodes[i] = callback.getResponseInfoWithChecks().getHttpStatusCode();
        }
        assertThat(statusCodes).asList().containsExactly(200, 404).inOrder();
    }

    @Test
    @SmallTest
    public void testInitTwoEnginesSimultaneously() throws Exception {
        // Threads will block on runBlocker to ensure simultaneous execution.
        ConditionVariable runBlocker = new ConditionVariable(false);
        RequestThread thread1 = new RequestThread(mUrl, runBlocker);
        RequestThread thread2 = new RequestThread(mUrl404, runBlocker);

        thread1.start();
        thread2.start();
        runBlocker.open();
        thread1.join();
        thread2.join();
        assertThat(thread1.mCallback.getResponseInfoWithChecks())
                .hasHttpStatusCodeThat()
                .isEqualTo(200);
        assertThat(thread2.mCallback.getResponseInfoWithChecks())
                .hasHttpStatusCodeThat()
                .isEqualTo(404);
    }

    @Test
    @SmallTest
    public void testInitTwoEnginesInSequence() throws Exception {
        ConditionVariable runBlocker = new ConditionVariable(true);
        RequestThread thread1 = new RequestThread(mUrl, runBlocker);
        RequestThread thread2 = new RequestThread(mUrl404, runBlocker);

        thread1.start();
        thread1.join();
        thread2.start();
        thread2.join();
        assertThat(thread1.mCallback.getResponseInfoWithChecks())
                .hasHttpStatusCodeThat()
                .isEqualTo(200);
        assertThat(thread2.mCallback.getResponseInfoWithChecks())
                .hasHttpStatusCodeThat()
                .isEqualTo(404);
    }

    @Test
    @SmallTest
    public void testInitDifferentEngines() throws Exception {
        // Test that concurrently instantiating Cronet context's upon various
        // different versions of the same Android Context does not cause crashes
        // like crbug.com/453845
        CronetEngine firstEngine =
                mTestRule
                        .getTestFramework()
                        .createNewSecondaryBuilder(mTestRule.getTestFramework().getContext())
                        .build();
        CronetEngine secondEngine =
                mTestRule
                        .getTestFramework()
                        .createNewSecondaryBuilder(mTestRule.getTestFramework().getContext())
                        .build();
        CronetEngine thirdEngine =
                mTestRule
                        .getTestFramework()
                        .createNewSecondaryBuilder(mTestRule.getTestFramework().getContext())
                        .build();
        firstEngine.shutdown();
        secondEngine.shutdown();
        thirdEngine.shutdown();
    }

    @Test
    @SmallTest
    @IgnoreFor(
            implementations = {CronetImplementation.FALLBACK, CronetImplementation.AOSP_PLATFORM},
            reason = "Global metrics delta is supported only by the native implementation")
    public void testGetGlobalMetricsDeltas() throws Exception {
        ExperimentalCronetEngine cronetEngine = mTestRule.getTestFramework().startEngine();

        byte[] delta1 = cronetEngine.getGlobalMetricsDeltas();

        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        UrlRequest.Builder builder =
                cronetEngine.newUrlRequestBuilder(mUrl, callback, callback.getExecutor());
        builder.build().start();
        callback.blockForDone();
        // Fetch deltas on a different thread the second time to make sure this is permitted.
        // See crbug.com/719448
        FutureTask<byte[]> task =
                new FutureTask<byte[]>(
                        new Callable<byte[]>() {
                            @Override
                            public byte[] call() {
                                return cronetEngine.getGlobalMetricsDeltas();
                            }
                        });
        new Thread(task).start();
        byte[] delta2 = task.get();
        assertThat(delta2).isNotEmpty();
        assertThat(delta2).isNotEqualTo(delta1);
    }

    @Test
    @SmallTest
    @IgnoreFor(
            implementations = {CronetImplementation.FALLBACK},
            reason = "Deliberate manual creation of native engines")
    public void testCronetEngineBuilderConfig() throws Exception {
        // This is to prompt load of native library.
        mTestRule.getTestFramework().startEngine();
        // Verify CronetEngine.Builder config is passed down accurately to native code.
        ExperimentalCronetEngine.Builder builder =
                new ExperimentalCronetEngine.Builder(mTestRule.getTestFramework().getContext());
        builder.enableHttp2(false);
        builder.enableQuic(true);
        builder.addQuicHint("example.com", 12, 34);
        builder.enableHttpCache(HTTP_CACHE_IN_MEMORY, 54321);
        builder.setUserAgent("efgh");
        builder.setExperimentalOptions("");
        builder.setStoragePath(getTestStorage(mTestRule.getTestFramework().getContext()));
        builder.enablePublicKeyPinningBypassForLocalTrustAnchors(false);
        CronetUrlRequestContextTestJni.get()
                .verifyUrlRequestContextConfig(
                        CronetUrlRequestContext.createNativeUrlRequestContextConfig(
                                CronetTestUtil.getCronetEngineBuilderImpl(builder)),
                        getTestStorage(mTestRule.getTestFramework().getContext()));
    }

    @Test
    @SmallTest
    @IgnoreFor(
            implementations = {CronetImplementation.FALLBACK},
            reason = "Deliberate manual creation of native engines")
    public void testCronetEngineQuicOffConfig() throws Exception {
        // This is to prompt load of native library.
        mTestRule.getTestFramework().startEngine();
        // Verify CronetEngine.Builder config is passed down accurately to native code.
        ExperimentalCronetEngine.Builder builder =
                new ExperimentalCronetEngine.Builder(mTestRule.getTestFramework().getContext());
        builder.enableHttp2(false);
        // QUIC is on by default. Disabling it here to make sure the built config can correctly
        // reflect the change.
        builder.enableQuic(false);
        builder.enableHttpCache(HTTP_CACHE_IN_MEMORY, 54321);
        builder.setExperimentalOptions("");
        builder.setUserAgent("efgh");
        builder.setStoragePath(getTestStorage(mTestRule.getTestFramework().getContext()));
        builder.enablePublicKeyPinningBypassForLocalTrustAnchors(false);
        CronetUrlRequestContextTestJni.get()
                .verifyUrlRequestContextQuicOffConfig(
                        CronetUrlRequestContext.createNativeUrlRequestContextConfig(
                                CronetTestUtil.getCronetEngineBuilderImpl(builder)),
                        getTestStorage(mTestRule.getTestFramework().getContext()));
    }

    private static class TestBadLibraryLoader extends CronetEngine.Builder.LibraryLoader {
        private boolean mWasCalled;

        @Override
        public void loadLibrary(String libName) {
            // Report that this method was called, but don't load the library
            mWasCalled = true;
        }

        boolean wasCalled() {
            return mWasCalled;
        }
    }

    @Test
    @SmallTest
    @IgnoreFor(
            implementations = {CronetImplementation.FALLBACK, CronetImplementation.AOSP_PLATFORM},
            reason = "LibraryLoader is supported only by the native implementation")
    public void testSetLibraryLoaderIsEnforcedByDefaultEmbeddedProvider() throws Exception {
        CronetEngine.Builder builder =
                new CronetEngine.Builder(mTestRule.getTestFramework().getContext());
        TestBadLibraryLoader loader = new TestBadLibraryLoader();
        builder.setLibraryLoader(loader);

        assertThrows(
                "Native library should not be loaded", UnsatisfiedLinkError.class, builder::build);
        assertThat(loader.wasCalled()).isTrue();

        // The init thread is started *before* the library is loaded, so the init thread is running
        // despite the library loading failure. Init thread initialization can race against test
        // cleanup (e.g. Context access). We work around the issue by ensuring test cleanup will
        // call shutdown() on a real engine, which will block until the init thread initialization
        // is done.
        mTestRule.getTestFramework().startEngine();
    }

    @Test
    @SmallTest
    @IgnoreFor(
            implementations = {CronetImplementation.FALLBACK, CronetImplementation.AOSP_PLATFORM},
            reason = "LibraryLoader is supported only by the native implementation")
    public void testSetLibraryLoaderIsIgnoredInNativeCronetEngineBuilderImpl() throws Exception {
        CronetEngine.Builder builder =
                new CronetEngine.Builder(
                        new NativeCronetEngineBuilderImpl(
                                mTestRule.getTestFramework().getContext()));
        TestBadLibraryLoader loader = new TestBadLibraryLoader();
        builder.setLibraryLoader(loader);
        CronetEngine engine = builder.build();
        assertThat(engine).isNotNull();
        assertThat(loader.wasCalled()).isFalse();
        engine.shutdown();
    }

    // Creates a CronetEngine on another thread and then one on the main thread.  This shouldn't
    // crash.
    @Test
    @SmallTest
    public void testThreadedStartup() throws Exception {
        final ConditionVariable otherThreadDone = new ConditionVariable();
        final ConditionVariable uiThreadDone = new ConditionVariable();
        new Handler(Looper.getMainLooper())
                .post(
                        new Runnable() {
                            @Override
                            public void run() {
                                final ExperimentalCronetEngine.Builder builder =
                                        mTestRule
                                                .getTestFramework()
                                                .createNewSecondaryBuilder(
                                                        mTestRule.getTestFramework().getContext());
                                new Thread() {
                                    @Override
                                    public void run() {
                                        CronetEngine cronetEngine = builder.build();
                                        otherThreadDone.open();
                                        cronetEngine.shutdown();
                                    }
                                }.start();
                                otherThreadDone.block();
                                builder.build().shutdown();
                                uiThreadDone.open();
                            }
                        });
        assertThat(uiThreadDone.block(1000)).isTrue();
    }

    @Test
    @SmallTest
    @IgnoreFor(
            implementations = {CronetImplementation.FALLBACK, CronetImplementation.AOSP_PLATFORM},
            reason = "JSON experimental options are supported only by the native implementation")
    public void testHostResolverRules() throws Exception {
        String resolverTestHostname = "some-weird-hostname";
        URL testUrl = new URL(mUrl);
        JSONObject hostResolverRules =
                new JSONObject()
                        .put(
                                "host_resolver_rules",
                                "MAP " + resolverTestHostname + " " + testUrl.getHost());
        mTestRule
                .getTestFramework()
                .applyEngineBuilderPatch(
                        (builder) -> {
                            JSONObject experimentalOptions =
                                    new JSONObject().put("HostResolverRules", hostResolverRules);
                            builder.setExperimentalOptions(experimentalOptions.toString());
                        });

        ExperimentalCronetEngine cronetEngine = mTestRule.getTestFramework().startEngine();

        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        URL requestUrl =
                new URL("http", resolverTestHostname, testUrl.getPort(), testUrl.getFile());
        UrlRequest.Builder urlRequestBuilder =
                cronetEngine.newUrlRequestBuilder(
                        requestUrl.toString(), callback, callback.getExecutor());
        urlRequestBuilder.build().start();
        callback.blockForDone();
        assertThat(callback.getResponseInfoWithChecks()).hasHttpStatusCodeThat().isEqualTo(200);
    }

    @Test
    @SmallTest
    @RequiresMinApi(6) // setThreadPriority added in API 6: crrev.com/472449
    public void testCronetEngineThreadPriority_invalidPriority_throwsException() {
        CronetEngine.Builder builder =
                mTestRule
                        .getTestFramework()
                        .createNewSecondaryBuilder(mTestRule.getTestFramework().getContext());
        // Try out of bounds thread priorities.
        IllegalArgumentException e =
                assertThrows(IllegalArgumentException.class, () -> builder.setThreadPriority(-21));
        assertThat(e).hasMessageThat().isEqualTo("Thread priority invalid");

        e = assertThrows(IllegalArgumentException.class, () -> builder.setThreadPriority(20));
        assertThat(e).hasMessageThat().isEqualTo("Thread priority invalid");
    }

    @Test
    @SmallTest
    @RequiresMinApi(6) // setThreadPriority added in API 6: crrev.com/472449
    public void testCronetEngineThreadPriority_viaUrlRequest_hasCorrectThreadPriority()
            throws Exception {
        // Test that valid thread priority range (-20..19) is working.
        for (int threadPriority = -20; threadPriority < 20; threadPriority++) {
            final FutureTask<Integer> task = getThreadPriorityTask();
            CronetEngine engine =
                    mTestRule
                            .getTestFramework()
                            .createNewSecondaryBuilder(mTestRule.getTestFramework().getContext())
                            .setThreadPriority(threadPriority)
                            .build();
            try {
                postToNetworkThread(engine, task);
                assertThat(task.get()).isEqualTo(threadPriority);
            } finally {
                engine.shutdown();
            }
        }
    }

    @Test
    @SmallTest
    @IgnoreFor(
            implementations = {CronetImplementation.FALLBACK, CronetImplementation.AOSP_PLATFORM},
            reason =
                    "Fallback implementation doesn't support HTTP flags; AOSP doesn't have this"
                        + " logic yet")
    public void testCronetEngineThreadPriority_honorsHttpFlag() throws Exception {
        final int flagValue = 13;
        mTestRule
                .getTestFramework()
                .setHttpFlags(
                        Flags.newBuilder()
                                .putFlags(
                                        CronetUrlRequestContext
                                                .OVERRIDE_NETWORK_THREAD_PRIORITY_FLAG_NAME,
                                        FlagValue.newBuilder()
                                                .addConstrainedValues(
                                                        FlagValue.ConstrainedValue.newBuilder()
                                                                .setIntValue(flagValue))
                                                .build())
                                .build());
        CronetEngine engine =
                mTestRule
                        .getTestFramework()
                        .createNewSecondaryBuilder(mTestRule.getTestFramework().getContext())
                        .build();
        try {
            final FutureTask<Integer> task = getThreadPriorityTask();
            postToNetworkThread(engine, task);
            assertThat(task.get()).isEqualTo(flagValue);
        } finally {
            engine.shutdown();
        }
    }

    @Test
    @SmallTest
    @RequiresMinApi(6) // setThreadPriority added in API 6: crrev.com/472449
    @IgnoreFor(
            implementations = {CronetImplementation.FALLBACK, CronetImplementation.AOSP_PLATFORM},
            reason =
                    "Fallback implementation doesn't support HTTP flags; AOSP doesn't have this"
                        + " logic yet")
    public void testCronetEngineThreadPriority_httpFlagOverridesBuilderOption() throws Exception {
        final int flagValue = 13;
        mTestRule
                .getTestFramework()
                .setHttpFlags(
                        Flags.newBuilder()
                                .putFlags(
                                        CronetUrlRequestContext
                                                .OVERRIDE_NETWORK_THREAD_PRIORITY_FLAG_NAME,
                                        FlagValue.newBuilder()
                                                .addConstrainedValues(
                                                        FlagValue.ConstrainedValue.newBuilder()
                                                                .setIntValue(flagValue))
                                                .build())
                                .build());
        CronetEngine engine =
                mTestRule
                        .getTestFramework()
                        .createNewSecondaryBuilder(mTestRule.getTestFramework().getContext())
                        .setThreadPriority(flagValue - 1)
                        .build();
        try {
            final FutureTask<Integer> task = getThreadPriorityTask();
            postToNetworkThread(engine, task);
            assertThat(task.get()).isEqualTo(flagValue);
        } finally {
            engine.shutdown();
        }
    }

    @Test
    @SmallTest
    @RequiresMinApi(6) // setThreadPriority added in API 6: crrev.com/472449
    @IgnoreFor(
            implementations = {CronetImplementation.FALLBACK},
            reason = "BidirectionalStream not supported by fallback.")
    public void testCronetEngineThreadPriority_viaBidirectionalStream_hasCorrectThreadPriority()
            throws Exception {
        // Test that valid thread priority range (-20..19) is working.
        for (int threadPriority = -20; threadPriority < 20; threadPriority++) {
            final FutureTask<Integer> task = getThreadPriorityTask();
            CronetEngine engine =
                    mTestRule
                            .getTestFramework()
                            .createNewSecondaryBuilder(mTestRule.getTestFramework().getContext())
                            .setThreadPriority(threadPriority)
                            .build();
            BidirectionalStream.Callback callback =
                    new TestBidirectionalStreamCallback.SimpleSucceedingCallback() {
                        @Override
                        public void onFailed(
                                BidirectionalStream request,
                                UrlResponseInfo responseInfo,
                                CronetException error) {
                            task.run();
                        }
                    };
            try {
                // Request an invalid URL with a direct executor which results in onFailed()
                // being called on the network thread.
                engine.newBidirectionalStreamBuilder("", callback, Runnable::run).build().start();
                assertThat(task.get()).isEqualTo(threadPriority);
            } finally {
                engine.shutdown();
            }
        }
    }

    /** Runs {@code r} on {@code engine}'s network thread. */
    private static void postToNetworkThread(final CronetEngine engine, final Runnable r) {
        // Works by requesting an invalid URL which results in onFailed() being called, which is
        // done through a direct executor which causes onFailed to be run on the network thread.
        UrlRequest.Callback callback =
                new TestUrlRequestCallback.SimpleSucceedingCallback() {
                    @Override
                    public void onFailed(
                            UrlRequest request,
                            UrlResponseInfo responseInfo,
                            CronetException error) {
                        r.run();
                    }
                };
        engine.newUrlRequestBuilder("", callback, Runnable::run).build().start();
    }

    /**
     * @returns the thread priority of {@code engine}'s network thread.
     */
    private static class ApiHelper {
        public static boolean doesContextExistForNetwork(CronetEngine engine, Network network)
                throws Exception {
            FutureTask<Boolean> task =
                    new FutureTask<Boolean>(
                            new Callable<Boolean>() {
                                @Override
                                public Boolean call() {
                                    return CronetTestUtil.doesURLRequestContextExistForTesting(
                                            engine, network);
                                }
                            });
            postToNetworkThread(engine, task);
            return task.get();
        }
    }

    /**
     * @returns the thread priority of {@code engine}'s network thread.
     */
    private FutureTask<Integer> getThreadPriorityTask() {
        return new FutureTask<Integer>(
                new Callable<Integer>() {
                    @Override
                    public Integer call() {
                        return Process.getThreadPriority(Process.myTid());
                    }
                });
    }

    /**
     * Cronet does not currently provide an API to wait for the active request count to change. We
     * can't just wait for the terminal callback to fire because Cronet updates the count some time
     * *after* we return from the callback. We hack around this by polling the active request count
     * in a loop.
     */
    private static void waitForActiveRequestCount(CronetEngine engine, int expectedCount)
            throws Exception {
        while (engine.getActiveRequestCount() != expectedCount) Thread.sleep(100);
    }

    @NativeMethods("cronet_tests")
    interface Natives {
        // Verifies that CronetEngine.Builder config from testCronetEngineBuilderConfig() is
        // properly translated to a native UrlRequestContextConfig.
        void verifyUrlRequestContextConfig(long config, String storagePath);

        // Verifies that CronetEngine.Builder config from testCronetEngineQuicOffConfig() is
        // properly translated to a native UrlRequestContextConfig and QUIC is turned off.
        void verifyUrlRequestContextQuicOffConfig(long config, String storagePath);
    }
}
