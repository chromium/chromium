// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net;

import static com.google.common.truth.Truth.assertThat;

import static org.junit.Assert.assertThrows;
import static org.junit.Assume.assumeTrue;

import static org.chromium.net.CronetEngine.Builder.HTTP_CACHE_IN_MEMORY;
import static org.chromium.net.CronetTestRule.getTestStorage;

import android.content.Context;
import android.net.Network;
import android.os.Build;
import android.os.ConditionVariable;
import android.os.Handler;
import android.os.Looper;
import android.os.Process;

import androidx.test.ext.junit.runners.AndroidJUnit4;
import androidx.test.filters.SmallTest;

import org.json.JSONObject;
import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.PathUtils;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.net.CronetTestRule.OnlyRunNativeCronet;
import org.chromium.net.CronetTestRule.RequiresMinAndroidApi;
import org.chromium.net.CronetTestRule.RequiresMinApi;
import org.chromium.net.NetworkChangeNotifierAutoDetect.ConnectivityManagerDelegate;
import org.chromium.net.TestUrlRequestCallback.ResponseStep;
import org.chromium.net.impl.CronetLibraryLoader;
import org.chromium.net.impl.CronetUrlRequestContext;
import org.chromium.net.impl.NativeCronetEngineBuilderImpl;
import org.chromium.net.test.EmbeddedTestServer;

import java.io.BufferedReader;
import java.io.File;
import java.io.FileReader;
import java.net.URL;
import java.nio.ByteBuffer;
import java.util.concurrent.Callable;
import java.util.concurrent.Executor;
import java.util.concurrent.FutureTask;
import java.util.concurrent.atomic.AtomicReference;

/**
 * Test CronetEngine.
 */
@RunWith(AndroidJUnit4.class)
@JNINamespace("cronet")
public class CronetUrlRequestContextTest {
    @Rule
    public final CronetTestRule mTestRule = CronetTestRule.withManualEngineStartup();

    // URLs used for tests.
    private static final String MOCK_CRONET_TEST_FAILED_URL =
            "http://mock.failed.request/-2";
    private static final String MOCK_CRONET_TEST_SUCCESS_URL =
            "http://mock.http/success.txt";
    private static final int MAX_FILE_SIZE = 1000000000;

    private EmbeddedTestServer mTestServer;
    private String mUrl;
    private String mUrl404;
    private String mUrl500;

    @Before
    public void setUp() throws Exception {
        mTestServer =
                EmbeddedTestServer.createAndStartServer(mTestRule.getTestFramework().getContext());
        mUrl = mTestServer.getURL("/echo?status=200");
        mUrl404 = mTestServer.getURL("/echo?status=404");
        mUrl500 = mTestServer.getURL("/echo?status=500");
    }

    @After
    public void tearDown() throws Exception {
        mTestServer.stopAndDestroyServer();
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
                    mTestRule.getTestFramework()
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

    /**
     * Callback that shutdowns the request context when request has succeeded
     * or failed.
     */
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

    @Test
    @SmallTest
    @SuppressWarnings("deprecation")
    public void testConfigUserAgent() throws Exception {
        String userAgentName = "User-Agent";
        String userAgentValue = "User-Agent-Value";

        mTestRule.getTestFramework().applyEngineBuilderPatch(
                (builder) -> builder.setUserAgent(userAgentValue));

        CronetEngine cronetEngine = mTestRule.getTestFramework().startEngine();
        NativeTestServer.shutdownNativeTestServer(); // startNativeTestServer returns false if it's
        // already running
        assertThat(
                NativeTestServer.startNativeTestServer(mTestRule.getTestFramework().getContext()))
                .isTrue();
        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        UrlRequest.Builder urlRequestBuilder = cronetEngine.newUrlRequestBuilder(
                NativeTestServer.getEchoHeaderURL(userAgentName), callback, callback.getExecutor());
        urlRequestBuilder.build().start();
        callback.blockForDone();
        assertThat(callback.mResponseAsString).isEqualTo(userAgentValue);
    }

    @Test
    @SmallTest
    // TODO: Remove the annotation after fixing http://crbug.com/637979 & http://crbug.com/637972
    @OnlyRunNativeCronet
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
        assertThat(e).hasMessageThat().isEqualTo("Cannot shutdown with running requests.");

        callback.waitForNextStep();
        assertThat(callback.mResponseStep).isEqualTo(ResponseStep.ON_RESPONSE_STARTED);

        e = assertThrows(Exception.class, cronetEngine::shutdown);
        assertThat(e).hasMessageThat().isEqualTo("Cannot shutdown with running requests.");

        callback.startNextRead(urlRequest);

        callback.waitForNextStep();

        assertThat(callback.mResponseStep).isEqualTo(ResponseStep.ON_READ_COMPLETED);
        e = assertThrows(Exception.class, cronetEngine::shutdown);
        assertThat(e).hasMessageThat().isEqualTo("Cannot shutdown with running requests.");

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
    @OnlyRunNativeCronet
    public void testShutdownDuringInit() throws Exception {
        final ConditionVariable block = new ConditionVariable(false);

        // Post a task to main thread to block until shutdown is called to test
        // scenario when shutdown is called right after construction before
        // context is fully initialized on the main thread.
        Runnable blockingTask = new Runnable() {
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
                (CronetUrlRequestContext) mTestRule.getTestFramework()
                        .createNewSecondaryBuilder(mTestRule.getTestFramework().getContext())
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
    @OnlyRunNativeCronet
    public void testInitAndShutdownOnMainThread() throws Exception {
        final ConditionVariable block = new ConditionVariable(false);

        // Post a task to main thread to init and shutdown on the main thread.
        Runnable blockingTask = new Runnable() {
            @Override
            public void run() {
                // Create new request context, loading the library.
                final CronetUrlRequestContext cronetEngine =
                        (CronetUrlRequestContext) mTestRule.getTestFramework()
                                .createNewSecondaryBuilder(
                                        mTestRule.getTestFramework().getContext())
                                .build();
                // Shutdown right after init.
                cronetEngine.shutdown();
                // Verify that context is shutdown.
                Exception e =
                        assertThrows(Exception.class, cronetEngine::getUrlRequestContextAdapter);
                assertThat(e).hasMessageThat().isEqualTo("Engine is shut down.");
                block.open();
            }
        };
        new Handler(Looper.getMainLooper()).post(blockingTask);
        // Wait for shutdown to complete on main thread.
        block.block();
    }

    @Test
    @SmallTest
    @OnlyRunNativeCronet // JavaCronetEngine doesn't support throwing on repeat shutdown()
    public void testMultipleShutdown() throws Exception {
        CronetEngine cronetEngine = mTestRule.getTestFramework().startEngine();
        cronetEngine.shutdown();
        Exception e = assertThrows(Exception.class, cronetEngine::shutdown);
        assertThat(e).hasMessageThat().isEqualTo("Engine is shut down.");
    }

    @Test
    @SmallTest
    // TODO: Remove the annotation after fixing http://crbug.com/637972
    @OnlyRunNativeCronet
    public void testShutdownAfterError() throws Exception {
        CronetEngine cronetEngine = mTestRule.getTestFramework().startEngine();
        ShutdownTestUrlRequestCallback callback = new ShutdownTestUrlRequestCallback(cronetEngine);
        UrlRequest.Builder urlRequestBuilder = cronetEngine.newUrlRequestBuilder(
                MOCK_CRONET_TEST_FAILED_URL, callback, callback.getExecutor());
        urlRequestBuilder.build().start();
        callback.blockForDone();
        assertThat(callback.mOnErrorCalled).isTrue();
        callback.blockForCallbackToComplete();
        callback.shutdownExecutor();
    }

    @Test
    @SmallTest
    @OnlyRunNativeCronet // JavaCronetEngine doesn't support throwing on shutdown()
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
        assertThat(e).hasMessageThat().isEqualTo("Cannot shutdown with running requests.");

        callback.waitForNextStep();
        assertThat(callback.mResponseStep).isEqualTo(ResponseStep.ON_RESPONSE_STARTED);
        urlRequest.cancel();
    }

    @Test
    @SmallTest
    @OnlyRunNativeCronet // Only chromium based Cronet supports the multi-network API
    @RequiresMinAndroidApi(Build.VERSION_CODES.M) // Multi-network API is supported from Marshmallow
    public void testNetworkBoundContextLifetime() throws Exception {
        // Multi-network API is available starting from Android Lollipop.
        ExperimentalCronetEngine cronetEngine = mTestRule.getTestFramework().startEngine();
        ConnectivityManagerDelegate delegate =
                new ConnectivityManagerDelegate(mTestRule.getTestFramework().getContext());
        Network defaultNetwork = delegate.getDefaultNetwork();
        assumeTrue(defaultNetwork != null);

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
        FutureTask<Void> task = new FutureTask<Void>(new Callable<Void>() {
            @Override
            public Void call() {
                NetworkChangeNotifier.fakeNetworkDisconnected(defaultNetwork.getNetworkHandle());
                return null;
            }
        });
        CronetLibraryLoader.postToInitThread(task);
        task.get();
        assertThat(ApiHelper.doesContextExistForNetwork(cronetEngine, defaultNetwork)).isFalse();
    }

    @Test
    @SmallTest
    @OnlyRunNativeCronet // Only chromium based Cronet supports the multi-network API
    @RequiresMinAndroidApi(Build.VERSION_CODES.M) // Multi-network API is supported from Marshmallow
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
        assumeTrue(defaultNetwork != null);

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
        CronetLibraryLoader.postToInitThread(new Runnable() {
            @Override
            public void run() {
                NetworkChangeNotifier.fakeNetworkDisconnected(defaultNetwork.getNetworkHandle());
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
    @SmallTest
    @OnlyRunNativeCronet // No netlogs for pure java impl
    public void testNetLog() throws Exception {
        Context context = mTestRule.getTestFramework().getContext();
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
    @OnlyRunNativeCronet // No netlogs for pure java impl
    public void testBoundedFileNetLog() throws Exception {
        Context context = mTestRule.getTestFramework().getContext();
        File directory = new File(PathUtils.getDataDirectory());
        File netLogDir = new File(directory, "NetLog");
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
    @OnlyRunNativeCronet // No netlogs for pure java impl
    // Tests that if stopNetLog is not explicity called, CronetEngine.shutdown()
    // will take care of it. crbug.com/623701.
    public void testNoStopNetLog() throws Exception {
        Context context = mTestRule.getTestFramework().getContext();
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
    @OnlyRunNativeCronet // No netlogs for pure java impl
    // Tests that if stopNetLog is not explicity called, CronetEngine.shutdown()
    // will take care of it. crbug.com/623701.
    public void testNoStopBoundedFileNetLog() throws Exception {
        Context context = mTestRule.getTestFramework().getContext();
        File directory = new File(PathUtils.getDataDirectory());
        File netLogDir = new File(directory, "NetLog");
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
    // TODO: the Java implementation currently fails this test - it incorrectly
    // increments the count on the second start.
    @OnlyRunNativeCronet
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
    // Only native Cronet has code paths that throw exceptions directly from start() on invalid
    // requests.
    @OnlyRunNativeCronet
    public void testGetActiveRequestCountOnInvalidRequest() throws Exception {
        ExperimentalCronetEngine cronetEngine = mTestRule.getTestFramework().startEngine();
        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        UrlRequest request = cronetEngine.newUrlRequestBuilder("", callback, callback.getExecutor())
                                     .setHttpMethod("")
                                     .build();
        assertThat(cronetEngine.getActiveRequestCount()).isEqualTo(0);
        assertThrows(Exception.class, request::start);
        assertThat(cronetEngine.getActiveRequestCount()).isEqualTo(0);
    }

    @Test
    @SmallTest
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
                cronetEngine.newUrlRequestBuilder(badUrl, callback1, callback1.getExecutor())
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
    // Request finished listeners are only supported by native Cronet.
    @OnlyRunNativeCronet
    public void testGetActiveRequestCountOnRequestFinishedListener() throws Exception {
        ExperimentalCronetEngine cronetEngine = mTestRule.getTestFramework().startEngine();
        TestRequestFinishedListener requestFinishedListener = new TestRequestFinishedListener();
        requestFinishedListener.blockListener();
        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        UrlRequest request =
                cronetEngine.newUrlRequestBuilder(mUrl, callback, callback.getExecutor())
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
    // Request finished listeners are only supported by native Cronet.
    @OnlyRunNativeCronet
    public void testGetActiveRequestCountOnThrowingRequestFinishedListener() throws Exception {
        ExperimentalCronetEngine cronetEngine = mTestRule.getTestFramework().startEngine();
        TestRequestFinishedListener requestFinishedListener = new TestRequestFinishedListener();
        requestFinishedListener.makeListenerThrow();
        requestFinishedListener.blockListener();
        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        UrlRequest request =
                cronetEngine.newUrlRequestBuilder(mUrl, callback, callback.getExecutor())
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
    // Request finished listeners are only supported by native Cronet.
    @OnlyRunNativeCronet
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
    // Request finished listeners are only supported by native Cronet.
    @OnlyRunNativeCronet
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
    @OnlyRunNativeCronet
    // Tests that NetLog contains events emitted by all live CronetEngines.
    public void testNetLogContainEventsFromAllLiveEngines() throws Exception {
        Context context = mTestRule.getTestFramework().getContext();
        File directory = new File(PathUtils.getDataDirectory());
        File file1 = File.createTempFile("cronet1", "json", directory);
        File file2 = File.createTempFile("cronet2", "json", directory);
        CronetEngine cronetEngine1 =
                mTestRule.getTestFramework()
                        .createNewSecondaryBuilder(mTestRule.getTestFramework().getContext())
                        .build();
        CronetEngine cronetEngine2 =
                mTestRule.getTestFramework()
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
    @OnlyRunNativeCronet
    // Tests that NetLog contains events emitted by all live CronetEngines.
    public void testBoundedFileNetLogContainEventsFromAllLiveEngines() throws Exception {
        Context context = mTestRule.getTestFramework().getContext();
        File directory = new File(PathUtils.getDataDirectory());
        File netLogDir1 = new File(directory, "NetLog1");
        assertThat(netLogDir1.exists()).isFalse();
        assertThat(netLogDir1.mkdir()).isTrue();
        File netLogDir2 = new File(directory, "NetLog2");
        assertThat(netLogDir2.exists()).isFalse();
        assertThat(netLogDir2.mkdir()).isTrue();
        File logFile1 = new File(netLogDir1, "netlog.json");
        File logFile2 = new File(netLogDir2, "netlog.json");

        CronetEngine cronetEngine1 =
                mTestRule.getTestFramework()
                        .createNewSecondaryBuilder(mTestRule.getTestFramework().getContext())
                        .build();
        CronetEngine cronetEngine2 =
                mTestRule.getTestFramework()
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
        CronetEngine.Builder builder = mTestRule.getTestFramework().createNewSecondaryBuilder(
                mTestRule.getTestFramework().getContext());
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
    @OnlyRunNativeCronet
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
        Executor directExecutor = new Executor() {
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
    @OnlyRunNativeCronet
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
        assertThat(callback.mResponseInfo.getHttpStatusCode()).isEqualTo(200);
        assertThat(callback.mResponseInfo.wasCached()).isTrue();
        assertThat(callback.mOnCanceledCalled).isTrue();
    }

    @Test
    @SmallTest
    @OnlyRunNativeCronet
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

        Exception e = assertThrows(
                Exception.class, () -> cronetEngine.startNetLogToFile(file.getPath(), false));
        assertThat(e).hasMessageThat().isEqualTo("Engine is shut down.");
        assertThat(hasBytesInNetLog(file)).isFalse();
        assertThat(file.delete()).isTrue();
        assertThat(file.exists()).isFalse();
    }

    @Test
    @SmallTest
    @OnlyRunNativeCronet
    public void testBoundedFileNetLogAfterShutdown() throws Exception {
        ExperimentalCronetEngine cronetEngine = mTestRule.getTestFramework().startEngine();
        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        UrlRequest.Builder urlRequestBuilder =
                cronetEngine.newUrlRequestBuilder(mUrl, callback, callback.getExecutor());
        urlRequestBuilder.build().start();
        callback.blockForDone();
        cronetEngine.shutdown();

        File directory = new File(PathUtils.getDataDirectory());
        File netLogDir = new File(directory, "NetLog");
        assertThat(netLogDir.exists()).isFalse();
        assertThat(netLogDir.mkdir()).isTrue();
        File logFile = new File(netLogDir, "netlog.json");
        Exception e = assertThrows(Exception.class,
                () -> cronetEngine.startNetLogToDisk(netLogDir.getPath(), false, MAX_FILE_SIZE));
        assertThat(e).hasMessageThat().isEqualTo("Engine is shut down.");
        assertThat(logFile.exists()).isFalse();
        FileUtils.recursivelyDeleteFile(netLogDir);
        assertThat(netLogDir.exists()).isFalse();
    }

    @Test
    @SmallTest
    @OnlyRunNativeCronet
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
    @OnlyRunNativeCronet
    public void testBoundedFileNetLogStartMultipleTimes() throws Exception {
        ExperimentalCronetEngine cronetEngine = mTestRule.getTestFramework().startEngine();
        File directory = new File(PathUtils.getDataDirectory());
        File netLogDir = new File(directory, "NetLog");
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
    @OnlyRunNativeCronet
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
    @OnlyRunNativeCronet
    public void testBoundedFileNetLogStopMultipleTimes() throws Exception {
        ExperimentalCronetEngine cronetEngine = mTestRule.getTestFramework().startEngine();
        File directory = new File(PathUtils.getDataDirectory());
        File netLogDir = new File(directory, "NetLog");
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
    @OnlyRunNativeCronet
    public void testNetLogWithBytes() throws Exception {
        Context context = mTestRule.getTestFramework().getContext();
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
    @OnlyRunNativeCronet
    public void testBoundedFileNetLogWithBytes() throws Exception {
        Context context = mTestRule.getTestFramework().getContext();
        File directory = new File(PathUtils.getDataDirectory());
        File netLogDir = new File(directory, "NetLog");
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
     * Helper method to make a request to {@code url}, wait for it to
     * complete, and check that the status code is the same as {@code expectedStatusCode}.
     */
    private void makeRequestAndCheckStatus(
            CronetEngine engine, String url, int expectedStatusCode) {
        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        UrlRequest request =
                engine.newUrlRequestBuilder(url, callback, callback.getExecutor()).build();
        request.start();
        callback.blockForDone();
        assertThat(callback.mResponseInfo.getHttpStatusCode()).isEqualTo(expectedStatusCode);
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
        assertThat(callback.mResponseInfo.wasCached()).isEqualTo(expectCached);
        assertThat(callback.mResponseAsString).isEqualTo("this is a cacheable file\n");
    }

    @Test
    @SmallTest
    @OnlyRunNativeCronet
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
    @OnlyRunNativeCronet // Not supported by Java implementation
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
    @OnlyRunNativeCronet // Not supported by Java implementation
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
    @OnlyRunNativeCronet
    public void testNoConcurrentDiskUsage() throws Exception {
        CronetEngine cronetEngine =
                createCronetEngineWithCache(CronetEngine.Builder.HTTP_CACHE_DISK);

        IllegalStateException e = assertThrows(IllegalStateException.class,
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
    @OnlyRunNativeCronet
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
    @OnlyRunNativeCronet // Broken for Java implementation
    public void testDisableCache() throws Exception {
        CronetEngine cronetEngine =
                createCronetEngineWithCache(CronetEngine.Builder.HTTP_CACHE_DISK);
        String url = NativeTestServer.getFileURL("/cacheable.txt");

        // When cache is disabled, making a request does not write to the cache.
        checkRequestCaching(cronetEngine, url, false, true /** disable cache */);
        checkRequestCaching(cronetEngine, url, false);

        // When cache is enabled, the second request is cached.
        checkRequestCaching(cronetEngine, url, false, true /** disable cache */);
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
        assertThat(callback.mError).isNotNull();
        assertThat(callback.mError)
                .hasMessageThat()
                .contains("Exception in CronetUrlRequest: net::ERR_CONNECTION_REFUSED");
        cronetEngine.shutdown();
    }

    @Test
    @SmallTest
    @OnlyRunNativeCronet // Broken for Java
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
                mTestRule.getTestFramework()
                        .enableDiskCache(mTestRule.getTestFramework().createNewSecondaryBuilder(
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
        assertThat(callback.mResponseInfo.getHttpStatusCode()).isEqualTo(200);
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
            statusCodes[i] = callback.mResponseInfo.getHttpStatusCode();
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
        assertThat(thread1.mCallback.mResponseInfo.getHttpStatusCode()).isEqualTo(200);
        assertThat(thread2.mCallback.mResponseInfo.getHttpStatusCode()).isEqualTo(404);
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
        assertThat(thread1.mCallback.mResponseInfo.getHttpStatusCode()).isEqualTo(200);
        assertThat(thread2.mCallback.mResponseInfo.getHttpStatusCode()).isEqualTo(404);
    }

    @Test
    @SmallTest
    public void testInitDifferentEngines() throws Exception {
        // Test that concurrently instantiating Cronet context's upon various
        // different versions of the same Android Context does not cause crashes
        // like crbug.com/453845
        CronetEngine firstEngine =
                mTestRule.getTestFramework()
                        .createNewSecondaryBuilder(mTestRule.getTestFramework().getContext())
                        .build();
        CronetEngine secondEngine =
                mTestRule.getTestFramework()
                        .createNewSecondaryBuilder(mTestRule.getTestFramework().getContext())
                        .build();
        CronetEngine thirdEngine =
                mTestRule.getTestFramework()
                        .createNewSecondaryBuilder(mTestRule.getTestFramework().getContext())
                        .build();
        firstEngine.shutdown();
        secondEngine.shutdown();
        thirdEngine.shutdown();
    }

    @Test
    @SmallTest
    @OnlyRunNativeCronet // Java engine doesn't produce metrics
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
        FutureTask<byte[]> task = new FutureTask<byte[]>(new Callable<byte[]>() {
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
    @OnlyRunNativeCronet // Deliberate manual creation of native engine
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
        CronetUrlRequestContextTestJni.get().verifyUrlRequestContextConfig(
                CronetUrlRequestContext.createNativeUrlRequestContextConfig(
                        CronetTestUtil.getCronetEngineBuilderImpl(builder)),
                getTestStorage(mTestRule.getTestFramework().getContext()));
    }

    @Test
    @SmallTest
    @OnlyRunNativeCronet // Deliberate manual creation of native engine
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
        CronetUrlRequestContextTestJni.get().verifyUrlRequestContextQuicOffConfig(
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
    @OnlyRunNativeCronet // Deliberate manual creation of native engine
    public void testSetLibraryLoaderIsEnforcedByDefaultEmbeddedProvider() throws Exception {
        CronetEngine.Builder builder =
                new CronetEngine.Builder(mTestRule.getTestFramework().getContext());
        TestBadLibraryLoader loader = new TestBadLibraryLoader();
        builder.setLibraryLoader(loader);

        assertThrows(
                "Native library should not be loaded", UnsatisfiedLinkError.class, builder::build);
        assertThat(loader.wasCalled()).isTrue();
    }

    @Test
    @SmallTest
    @OnlyRunNativeCronet
    public void testSetLibraryLoaderIsIgnoredInNativeCronetEngineBuilderImpl() throws Exception {
        CronetEngine.Builder builder = new CronetEngine.Builder(
                new NativeCronetEngineBuilderImpl(mTestRule.getTestFramework().getContext()));
        TestBadLibraryLoader loader = new TestBadLibraryLoader();
        builder.setLibraryLoader(loader);
        CronetEngine engine = builder.build();
        assertThat(engine).isNotNull();
        assertThat(loader.wasCalled()).isFalse();
    }

    // Creates a CronetEngine on another thread and then one on the main thread.  This shouldn't
    // crash.
    @Test
    @SmallTest
    public void testThreadedStartup() throws Exception {
        final ConditionVariable otherThreadDone = new ConditionVariable();
        final ConditionVariable uiThreadDone = new ConditionVariable();
        new Handler(Looper.getMainLooper()).post(new Runnable() {
            @Override
            public void run() {
                final ExperimentalCronetEngine.Builder builder =
                        mTestRule.getTestFramework().createNewSecondaryBuilder(
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
    @OnlyRunNativeCronet // Java implementation doesn't support experimental options
    public void testHostResolverRules() throws Exception {
        String resolverTestHostname = "some-weird-hostname";
        URL testUrl = new URL(mUrl);
        mTestRule.getTestFramework().applyEngineBuilderPatch((builder) -> {
            JSONObject hostResolverRules = new JSONObject().put(
                    "host_resolver_rules", "MAP " + resolverTestHostname + " " + testUrl.getHost());
            JSONObject experimentalOptions =
                    new JSONObject().put("HostResolverRules", hostResolverRules);
            builder.setExperimentalOptions(experimentalOptions.toString());
        });

        ExperimentalCronetEngine cronetEngine = mTestRule.getTestFramework().startEngine();

        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        URL requestUrl =
                new URL("http", resolverTestHostname, testUrl.getPort(), testUrl.getFile());
        UrlRequest.Builder urlRequestBuilder = cronetEngine.newUrlRequestBuilder(
                requestUrl.toString(), callback, callback.getExecutor());
        urlRequestBuilder.build().start();
        callback.blockForDone();
        assertThat(callback.mResponseInfo.getHttpStatusCode()).isEqualTo(200);
    }

    /**
     * Runs {@code r} on {@code engine}'s network thread.
     */
    private static void postToNetworkThread(final CronetEngine engine, final Runnable r) {
        // Works by requesting an invalid URL which results in onFailed() being called, which is
        // done through a direct executor which causes onFailed to be run on the network thread.
        Executor directExecutor = new Executor() {
            @Override
            public void execute(Runnable runable) {
                runable.run();
            }
        };
        UrlRequest.Callback callback = new UrlRequest.Callback() {
            @Override
            public void onRedirectReceived(
                    UrlRequest request, UrlResponseInfo responseInfo, String newLocationUrl) {}
            @Override
            public void onResponseStarted(UrlRequest request, UrlResponseInfo responseInfo) {}
            @Override
            public void onReadCompleted(
                    UrlRequest request, UrlResponseInfo responseInfo, ByteBuffer byteBuffer) {}
            @Override
            public void onSucceeded(UrlRequest request, UrlResponseInfo responseInfo) {}

            @Override
            public void onFailed(
                    UrlRequest request, UrlResponseInfo responseInfo, CronetException error) {
                r.run();
            }
        };
        engine.newUrlRequestBuilder("", callback, directExecutor).build().start();
    }

    /**
     * @returns the thread priority of {@code engine}'s network thread.
     */
    private static class ApiHelper {
        public static boolean doesContextExistForNetwork(CronetEngine engine, Network network)
                throws Exception {
            FutureTask<Boolean> task = new FutureTask<Boolean>(new Callable<Boolean>() {
                @Override
                public Boolean call() {
                    return CronetTestUtil.doesURLRequestContextExistForTesting(engine, network);
                }
            });
            postToNetworkThread(engine, task);
            return task.get();
        }
    }

    /**
     * @returns the thread priority of {@code engine}'s network thread.
     */
    private int getThreadPriority(CronetEngine engine) throws Exception {
        FutureTask<Integer> task = new FutureTask<Integer>(new Callable<Integer>() {
            @Override
            public Integer call() {
                return Process.getThreadPriority(Process.myTid());
            }
        });
        postToNetworkThread(engine, task);
        return task.get();
    }

    /**
     * Cronet does not currently provide an API to wait for the active request
     * count to change. We can't just wait for the terminal callback to fire
     * because Cronet updates the count some time *after* we return from the
     * callback. We hack around this by polling the active request count in a
     * loop.
     */
    private static void waitForActiveRequestCount(CronetEngine engine, int expectedCount)
            throws Exception {
        while (engine.getActiveRequestCount() != expectedCount) Thread.sleep(100);
    }

    @Test
    @SmallTest
    @RequiresMinApi(6) // setThreadPriority added in API 6: crrev.com/472449
    public void testCronetEngineThreadPriority() throws Exception {
        ExperimentalCronetEngine.Builder builder =
                mTestRule.getTestFramework().createNewSecondaryBuilder(
                        mTestRule.getTestFramework().getContext());
        // Try out of bounds thread priorities.
        IllegalArgumentException e =
                assertThrows(IllegalArgumentException.class, () -> builder.setThreadPriority(-21));
        assertThat(e).hasMessageThat().isEqualTo("Thread priority invalid");

        e = assertThrows(IllegalArgumentException.class, () -> builder.setThreadPriority(20));
        assertThat(e).hasMessageThat().isEqualTo("Thread priority invalid");

        // Test that valid thread priority range (-20..19) is working.
        for (int threadPriority = -20; threadPriority < 20; threadPriority++) {
            builder.setThreadPriority(threadPriority);
            CronetEngine engine = builder.build();
            try {
                assertThat(getThreadPriority(engine)).isEqualTo(threadPriority);
            } finally {
                engine.shutdown();
            }
        }
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
