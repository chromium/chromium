// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net;

import static com.google.common.truth.Truth.assertThat;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import static org.chromium.net.CronetTestRule.getContext;

import androidx.test.ext.junit.runners.AndroidJUnit4;
import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.net.CronetTestRule.CronetTestFramework;
import org.chromium.net.CronetTestRule.OnlyRunNativeCronet;
import org.chromium.net.impl.CronetUploadDataStream;
import org.chromium.net.impl.CronetUrlRequest;

import java.util.Arrays;
import java.util.List;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;

/**
 * Tests that directly drive {@code CronetUploadDataStream} and
 * {@code UploadDataProvider} to simulate different ordering of reset, init,
 * read, and rewind calls.
 */
@RunWith(AndroidJUnit4.class)
public class CronetUploadTest {
    @Rule
    public final CronetTestRule mTestRule = new CronetTestRule();

    private TestDrivenDataProvider mDataProvider;
    private CronetUploadDataStream mUploadDataStream;
    private TestUploadDataStreamHandler mHandler;
    private CronetTestFramework mTestFramework;

    @Before
    @SuppressWarnings({"PrimitiveArrayPassedToVarargsMethod", "ArraysAsListPrimitiveArray"})
    public void setUp() throws Exception {
        mTestFramework = mTestRule.startCronetTestFramework();
        ExecutorService executor = Executors.newSingleThreadExecutor();
        List<byte[]> reads = Arrays.asList("hello".getBytes());
        mDataProvider = new TestDrivenDataProvider(executor, reads);

        // Creates a dummy CronetUrlRequest, which is not used to drive CronetUploadDataStream.
        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        UrlRequest.Builder builder = mTestFramework.mCronetEngine.newUrlRequestBuilder(
                "https://dummy.url", callback, callback.getExecutor());
        UrlRequest urlRequest = builder.build();

        mUploadDataStream =
                new CronetUploadDataStream(mDataProvider, executor, (CronetUrlRequest) urlRequest);
        mHandler = new TestUploadDataStreamHandler(
                getContext(), mUploadDataStream.createUploadDataStreamForTesting());
    }

    @After
    public void tearDown() throws Exception {
        // Destroy handler's native objects.
        mHandler.destroyNativeObjects();
    }

    /**
     * Tests that after some data is read, init triggers a rewind, and that
     * before the rewind completes, init blocks.
     */
    @Test
    @SmallTest
    @OnlyRunNativeCronet
    public void testInitTriggersRewindAndInitBeforeRewindCompletes() throws Exception {
        // Init completes synchronously and read succeeds.
        assertTrue(mHandler.init());
        mHandler.read();
        mDataProvider.waitForReadRequest();
        mHandler.checkReadCallbackNotInvoked();
        mDataProvider.onReadSucceeded(mUploadDataStream);
        mHandler.waitForReadComplete();
        mDataProvider.assertReadNotPending();
        assertThat(mDataProvider.getNumRewindCalls()).isEqualTo(0);
        assertThat(mDataProvider.getNumReadCalls()).isEqualTo(1);
        assertThat(mHandler.getData()).isEqualTo("hello");

        // Reset and then init, which should trigger a rewind.
        mHandler.reset();
        assertThat(mHandler.getData()).isEmpty();
        assertFalse(mHandler.init());
        mDataProvider.waitForRewindRequest();
        mHandler.checkInitCallbackNotInvoked();

        // Before rewind completes, reset and init should block.
        mHandler.reset();
        assertFalse(mHandler.init());

        // Signal rewind completes, and wait for init to complete.
        mHandler.checkInitCallbackNotInvoked();
        mDataProvider.onRewindSucceeded(mUploadDataStream);
        mHandler.waitForInitComplete();
        mDataProvider.assertRewindNotPending();

        // Read should complete successfully since init has completed.
        mHandler.read();
        mDataProvider.waitForReadRequest();
        mHandler.checkReadCallbackNotInvoked();
        mDataProvider.onReadSucceeded(mUploadDataStream);
        mHandler.waitForReadComplete();
        mDataProvider.assertReadNotPending();
        assertThat(mDataProvider.getNumRewindCalls()).isEqualTo(1);
        assertThat(mDataProvider.getNumReadCalls()).isEqualTo(2);
        assertThat(mHandler.getData()).isEqualTo("hello");
    }

    /**
     * Tests that after some data is read, init triggers a rewind, and that
     * after the rewind completes, init does not block.
     */
    @Test
    @SmallTest
    @OnlyRunNativeCronet
    public void testInitTriggersRewindAndInitAfterRewindCompletes() throws Exception {
        // Init completes synchronously and read succeeds.
        assertTrue(mHandler.init());
        mHandler.read();
        mDataProvider.waitForReadRequest();
        mHandler.checkReadCallbackNotInvoked();
        mDataProvider.onReadSucceeded(mUploadDataStream);
        mHandler.waitForReadComplete();
        mDataProvider.assertReadNotPending();
        assertThat(mDataProvider.getNumRewindCalls()).isEqualTo(0);
        assertThat(mDataProvider.getNumReadCalls()).isEqualTo(1);
        assertThat(mHandler.getData()).isEqualTo("hello");

        // Reset and then init, which should trigger a rewind.
        mHandler.reset();
        assertThat(mHandler.getData()).isEmpty();
        assertFalse(mHandler.init());
        mDataProvider.waitForRewindRequest();
        mHandler.checkInitCallbackNotInvoked();

        // Signal rewind completes, and wait for init to complete.
        mDataProvider.onRewindSucceeded(mUploadDataStream);
        mHandler.waitForInitComplete();
        mDataProvider.assertRewindNotPending();

        // Reset and init should not block, since rewind has completed.
        mHandler.reset();
        assertTrue(mHandler.init());

        // Read should complete successfully since init has completed.
        mHandler.read();
        mDataProvider.waitForReadRequest();
        mHandler.checkReadCallbackNotInvoked();
        mDataProvider.onReadSucceeded(mUploadDataStream);
        mHandler.waitForReadComplete();
        mDataProvider.assertReadNotPending();
        assertThat(mDataProvider.getNumRewindCalls()).isEqualTo(1);
        assertThat(mDataProvider.getNumReadCalls()).isEqualTo(2);
        assertThat(mHandler.getData()).isEqualTo("hello");
    }

    /**
     * Tests that if init before read completes, a rewind is triggered when
     * read completes.
     */
    @Test
    @SmallTest
    @OnlyRunNativeCronet
    public void testReadCompleteTriggerRewind() throws Exception {
        // Reset and init before read completes.
        assertTrue(mHandler.init());
        mHandler.read();
        mDataProvider.waitForReadRequest();
        mHandler.checkReadCallbackNotInvoked();
        mHandler.reset();
        // Init should return asynchronously, since there is a pending read.
        assertFalse(mHandler.init());
        mDataProvider.assertRewindNotPending();
        mHandler.checkInitCallbackNotInvoked();
        assertThat(mDataProvider.getNumRewindCalls()).isEqualTo(0);
        assertThat(mDataProvider.getNumReadCalls()).isEqualTo(1);
        assertThat(mHandler.getData()).isEmpty();

        // Read completes should trigger a rewind.
        mDataProvider.onReadSucceeded(mUploadDataStream);
        mDataProvider.waitForRewindRequest();
        mHandler.checkInitCallbackNotInvoked();
        mDataProvider.onRewindSucceeded(mUploadDataStream);
        mHandler.waitForInitComplete();
        mDataProvider.assertRewindNotPending();
        assertThat(mDataProvider.getNumRewindCalls()).isEqualTo(1);
        assertThat(mDataProvider.getNumReadCalls()).isEqualTo(1);
        assertThat(mHandler.getData()).isEmpty();
    }

    /**
     * Tests that when init again after rewind completes, no additional rewind
     * is triggered. This test is the same as testReadCompleteTriggerRewind
     * except that this test invokes reset and init again in the end.
     */
    @Test
    @SmallTest
    @OnlyRunNativeCronet
    public void testReadCompleteTriggerRewindOnlyOneRewind() throws Exception {
        testReadCompleteTriggerRewind();
        // Reset and Init again, no rewind should happen.
        mHandler.reset();
        assertTrue(mHandler.init());
        mDataProvider.assertRewindNotPending();
        assertThat(mDataProvider.getNumRewindCalls()).isEqualTo(1);
        assertThat(mDataProvider.getNumReadCalls()).isEqualTo(1);
        assertThat(mHandler.getData()).isEmpty();
    }

    /**
     * Tests that if reset before read completes, no rewind is triggered, and
     * that a following init triggers rewind.
     */
    @Test
    @SmallTest
    @OnlyRunNativeCronet
    public void testResetBeforeReadCompleteAndInitTriggerRewind() throws Exception {
        // Reset before read completes. Rewind is not triggered.
        assertTrue(mHandler.init());
        mHandler.read();
        mDataProvider.waitForReadRequest();
        mHandler.checkReadCallbackNotInvoked();
        mHandler.reset();
        mDataProvider.onReadSucceeded(mUploadDataStream);
        mDataProvider.assertRewindNotPending();
        assertThat(mDataProvider.getNumRewindCalls()).isEqualTo(0);
        assertThat(mDataProvider.getNumReadCalls()).isEqualTo(1);
        assertThat(mHandler.getData()).isEmpty();

        // Init should trigger a rewind.
        assertFalse(mHandler.init());
        mDataProvider.waitForRewindRequest();
        mHandler.checkInitCallbackNotInvoked();
        mDataProvider.onRewindSucceeded(mUploadDataStream);
        mHandler.waitForInitComplete();
        mDataProvider.assertRewindNotPending();
        assertThat(mDataProvider.getNumRewindCalls()).isEqualTo(1);
        assertThat(mDataProvider.getNumReadCalls()).isEqualTo(1);
        assertThat(mHandler.getData()).isEmpty();
    }

    /**
     * Tests that there is no crash when native CronetUploadDataStream is
     * destroyed while read is pending. The test is racy since the read could
     * complete either before or after the Java CronetUploadDataStream's
     * onDestroyUploadDataStream() method is invoked. However, the test should
     * pass either way, though we are interested in the latter case.
     */
    @Test
    @SmallTest
    @OnlyRunNativeCronet
    public void testDestroyNativeStreamBeforeReadComplete() throws Exception {
        // Start a read and wait for it to be pending.
        assertTrue(mHandler.init());
        mHandler.read();
        mDataProvider.waitForReadRequest();
        mHandler.checkReadCallbackNotInvoked();

        // Destroy the C++ TestUploadDataStreamHandler. The handler will then
        // destroy the C++ CronetUploadDataStream it owns on the network thread.
        // That will result in calling the Java CronetUploadDataSteam's
        // onUploadDataStreamDestroyed() method on its executor thread, which
        // will then destroy the CronetUploadDataStreamAdapter.
        mHandler.destroyNativeObjects();

        // Make the read complete should not encounter a crash.
        mDataProvider.onReadSucceeded(mUploadDataStream);

        assertThat(mDataProvider.getNumRewindCalls()).isEqualTo(0);
        assertThat(mDataProvider.getNumReadCalls()).isEqualTo(1);
    }

    /**
     * Tests that there is no crash when native CronetUploadDataStream is
     * destroyed while rewind is pending. The test is racy since rewind could
     * complete either before or after the Java CronetUploadDataStream's
     * onDestroyUploadDataStream() method is invoked. However, the test should
     * pass either way, though we are interested in the latter case.
     */
    @Test
    @SmallTest
    @OnlyRunNativeCronet
    public void testDestroyNativeStreamBeforeRewindComplete() throws Exception {
        // Start a read and wait for it to complete.
        assertTrue(mHandler.init());
        mHandler.read();
        mDataProvider.waitForReadRequest();
        mHandler.checkReadCallbackNotInvoked();
        mDataProvider.onReadSucceeded(mUploadDataStream);
        mHandler.waitForReadComplete();
        mDataProvider.assertReadNotPending();
        assertThat(mDataProvider.getNumRewindCalls()).isEqualTo(0);
        assertThat(mDataProvider.getNumReadCalls()).isEqualTo(1);
        assertThat(mHandler.getData()).isEqualTo("hello");

        // Reset and then init, which should trigger a rewind.
        mHandler.reset();
        assertThat(mHandler.getData()).isEmpty();
        assertFalse(mHandler.init());
        mDataProvider.waitForRewindRequest();
        mHandler.checkInitCallbackNotInvoked();

        // Destroy the C++ TestUploadDataStreamHandler. The handler will then
        // destroy the C++ CronetUploadDataStream it owns on the network thread.
        // That will result in calling the Java CronetUploadDataSteam's
        // onUploadDataStreamDestroyed() method on its executor thread, which
        // will then destroy the CronetUploadDataStreamAdapter.
        mHandler.destroyNativeObjects();

        // Signal rewind completes, and wait for init to complete.
        mDataProvider.onRewindSucceeded(mUploadDataStream);

        assertThat(mDataProvider.getNumRewindCalls()).isEqualTo(1);
        assertThat(mDataProvider.getNumReadCalls()).isEqualTo(1);
    }
}
