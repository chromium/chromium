// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net;

import android.app.Activity;
import android.net.Uri;
import android.os.Bundle;
import android.os.Debug;

import org.json.JSONException;
import org.json.JSONObject;

import org.chromium.base.ContextUtils;
import org.chromium.base.PathUtils;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.net.HttpURLConnection;
import java.net.MalformedURLException;
import java.net.URL;
import java.net.URLConnection;
import java.nio.ByteBuffer;
import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.Callable;
import java.util.concurrent.Executor;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;
import java.util.concurrent.Future;
import java.util.concurrent.LinkedBlockingQueue;
import java.util.concurrent.TimeUnit;

/** Runs networking benchmarks and saves results to a file. */
public class CronetPerfTestActivity extends Activity {
    private static final String PRIVATE_DATA_DIRECTORY_SUFFIX = "cronet_perf_test";
    // Benchmark configuration passed down from host via Intent data.
    // Call getConfig*(key) to extract individual configuration values.
    private Uri mConfig;

    // Functions that retrieve individual benchmark configuration values.
    private String getConfigString(String key) {
        return mConfig.getQueryParameter(key);
    }

    private int getConfigInt(String key) {
        return Integer.parseInt(mConfig.getQueryParameter(key));
    }

    private boolean getConfigBoolean(String key) {
        return Boolean.parseBoolean(mConfig.getQueryParameter(key));
    }

    private enum Mode {
        SYSTEM_HUC, // Benchmark system HttpURLConnection
        CRONET_HUC, // Benchmark Cronet's HttpURLConnection
        CRONET_ASYNC, // Benchmark Cronet's asynchronous API
    }

    private enum Direction {
        UP, // Benchmark upload (i.e. POST)
        DOWN, // Benchmark download (i.e. GET)
    }

    private enum Size {
        LARGE, // Large benchmark
        SMALL, // Small benchmark
    }

    private enum Protocol {
        HTTP,
        QUIC,
    }

    // Put together a benchmark configuration into a benchmark name.
    // Make it fixed length for more readable tables.
    // Benchmark names are written to the JSON output file and slurped up by Telemetry on the host.
    private static String buildBenchmarkName(
            Mode mode, Direction direction, Protocol protocol, int concurrency, int iterations) {
        String name = direction == Direction.UP ? "Up___" : "Down_";
        switch (protocol) {
            case HTTP:
                name += "H_";
                break;
            case QUIC:
                name += "Q_";
                break;
            default:
                throw new IllegalArgumentException("Unknown protocol: " + protocol);
        }
        name += iterations + "_" + concurrency + "_";
        switch (mode) {
            case SYSTEM_HUC:
                name += "SystemHUC__";
                break;
            case CRONET_HUC:
                name += "CronetHUC__";
                break;
            case CRONET_ASYNC:
                name += "CronetAsync";
                break;
            default:
                throw new IllegalArgumentException("Unknown mode: " + mode);
        }
        return name;
    }

    // Responsible for running one particular benchmark and timing it.
    private class Benchmark {
        private final Mode mMode;
        private final Direction mDirection;
        private final Protocol mProtocol;
        private final URL mUrl;
        private final String mName;
        private final CronetEngine mCronetEngine;
        // Size in bytes of content being uploaded or downloaded.
        private final int mLength;
        // How many requests to execute.
        private final int mIterations;
        // How many requests to execute in parallel at any one time.
        private final int mConcurrency;
        // Dictionary of benchmark names mapped to times to complete the benchmarks.
        private final JSONObject mResults;
        // How large a buffer to use for passing content, in bytes.
        private final int mBufferSize;
        // Cached copy of getConfigBoolean("CRONET_ASYNC_USE_NETWORK_THREAD") for faster access.
        private final boolean mUseNetworkThread;

        private long mStartTimeMs = -1;
        private long mStopTimeMs = -1;

        /**
         * Create a new benchmark to run.  Sets up various configuration settings.
         * @param mode The API to benchmark.
         * @param direction The transfer direction to benchmark (i.e. upload or download).
         * @param size The size of the transfers to benchmark (i.e. large or small).
         * @param protocol The transfer protocol to benchmark (i.e. HTTP or QUIC).
         * @param concurrency The number of transfers to perform concurrently.
         * @param results Mapping of benchmark names to time required to run the benchmark in ms.
         *                When the benchmark completes this is updated with the result.
         */
        public Benchmark(
                Mode mode,
                Direction direction,
                Size size,
                Protocol protocol,
                int concurrency,
                JSONObject results) {
            mMode = mode;
            mDirection = direction;
            mProtocol = protocol;
            final String resource;
            switch (size) {
                case SMALL:
                    resource = getConfigString("SMALL_RESOURCE");
                    mIterations = getConfigInt("SMALL_ITERATIONS");
                    mLength = getConfigInt("SMALL_RESOURCE_SIZE");
                    break;
                case LARGE:
                    // When measuring a large upload, only download a small amount so download time
                    // isn't significant.
                    resource =
                            getConfigString(
                                    direction == Direction.UP
                                            ? "SMALL_RESOURCE"
                                            : "LARGE_RESOURCE");
                    mIterations = getConfigInt("LARGE_ITERATIONS");
                    mLength = getConfigInt("LARGE_RESOURCE_SIZE");
                    break;
                default:
                    throw new IllegalArgumentException("Unknown size: " + size);
            }
            final String scheme;
            final String host;
            final int port;
            switch (protocol) {
                case HTTP:
                    scheme = "http";
                    host = getConfigString("HOST_IP");
                    port = getConfigInt("HTTP_PORT");
                    break;
                case QUIC:
                    scheme = "https";
                    host = getConfigString("HOST");
                    port = getConfigInt("QUIC_PORT");
                    break;
                default:
                    throw new IllegalArgumentException("Unknown protocol: " + protocol);
            }
            try {
                mUrl = new URL(scheme, host, port, resource);
            } catch (MalformedURLException e) {
                throw new IllegalArgumentException(
                        "Bad URL: " + host + ":" + port + "/" + resource);
            }
            final ExperimentalCronetEngine.Builder cronetEngineBuilder =
                    new ExperimentalCronetEngine.Builder(CronetPerfTestActivity.this);
            System.loadLibrary("cronet_tests");
            if (mProtocol == Protocol.QUIC) {
                cronetEngineBuilder.enableQuic(true);
                cronetEngineBuilder.addQuicHint(host, port, port);
                CronetTestUtil.setMockCertVerifierForTesting(
                        cronetEngineBuilder,
                        MockCertVerifier.createMockCertVerifier(
                                new String[] {getConfigString("QUIC_CERT_FILE")}, true));
            }

            try {
                JSONObject hostResolverParams =
                        CronetTestUtil.generateHostResolverRules(getConfigString("HOST_IP"));
                JSONObject experimentalOptions =
                        new JSONObject().put("HostResolverRules", hostResolverParams);
                cronetEngineBuilder.setExperimentalOptions(experimentalOptions.toString());
            } catch (JSONException e) {
                throw new IllegalStateException("JSON failed: " + e);
            }
            mCronetEngine = cronetEngineBuilder.build();
            mName = buildBenchmarkName(mode, direction, protocol, concurrency, mIterations);
            mConcurrency = concurrency;
            mResults = results;
            mBufferSize =
                    mLength > getConfigInt("MAX_BUFFER_SIZE")
                            ? getConfigInt("MAX_BUFFER_SIZE")
                            : mLength;
            mUseNetworkThread = getConfigBoolean("CRONET_ASYNC_USE_NETWORK_THREAD");
        }

        private void startTimer() {
            mStartTimeMs = System.currentTimeMillis();
        }

        private void stopTimer() {
            mStopTimeMs = System.currentTimeMillis();
        }

        private void reportResult() {
            if (mStartTimeMs == -1 || mStopTimeMs == -1) {
                throw new IllegalStateException("startTimer() or stopTimer() not called");
            }
            try {
                mResults.put(mName, mStopTimeMs - mStartTimeMs);
            } catch (JSONException e) {
                System.out.println("Failed to write JSON result for " + mName);
            }
        }

        private void startLogging() {
            if (getConfigBoolean("CAPTURE_NETLOG")) {
                mCronetEngine.startNetLogToFile(
                        getFilesDir().getPath() + "/" + mName + ".json", false);
            }
            if (getConfigBoolean("CAPTURE_TRACE")) {
                Debug.startMethodTracing(getFilesDir().getPath() + "/" + mName + ".trace");
            } else if (getConfigBoolean("CAPTURE_SAMPLED_TRACE")) {
                Debug.startMethodTracingSampling(
                        getFilesDir().getPath() + "/" + mName + ".trace", 8000000, 10);
            }
        }

        private void stopLogging() {
            if (getConfigBoolean("CAPTURE_NETLOG")) {
                mCronetEngine.stopNetLog();
            }
            if (getConfigBoolean("CAPTURE_TRACE") || getConfigBoolean("CAPTURE_SAMPLED_TRACE")) {
                Debug.stopMethodTracing();
            }
        }

        /**
         * Transfer {@code mLength} bytes through HttpURLConnection in {@code mDirection} direction.
         * @param urlConnection The HttpURLConnection to use for transfer.
         * @param buffer A buffer of length |mBufferSize| to use for transfer.
         * @return {@code true} if transfer completed successfully.
         */
        private boolean exerciseHttpURLConnection(URLConnection urlConnection, byte[] buffer)
                throws IOException {
            final HttpURLConnection connection = (HttpURLConnection) urlConnection;
            try {
                int bytesTransfered = 0;
                if (mDirection == Direction.DOWN) {
                    final InputStream inputStream = connection.getInputStream();
                    while (true) {
                        final int bytesRead = inputStream.read(buffer, 0, mBufferSize);
                        if (bytesRead == -1) {
                            break;
                        } else {
                            bytesTransfered += bytesRead;
                        }
                    }
                } else {
                    connection.setDoOutput(true);
                    connection.setRequestMethod("POST");
                    connection.setRequestProperty("Content-Length", Integer.toString(mLength));
                    final OutputStream outputStream = connection.getOutputStream();
                    for (int remaining = mLength; remaining > 0; remaining -= mBufferSize) {
                        outputStream.write(buffer, 0, Math.min(remaining, mBufferSize));
                    }
                    bytesTransfered = mLength;
                }
                return connection.getResponseCode() == 200 && bytesTransfered == mLength;
            } finally {
                connection.disconnect();
            }
        }

        // GET or POST to one particular URL using URL.openConnection()
        private class SystemHttpURLConnectionFetchTask implements Callable<Boolean> {
            private final byte[] mBuffer = new byte[mBufferSize];

            @Override
            public Boolean call() {
                try {
                    return exerciseHttpURLConnection(mUrl.openConnection(), mBuffer);
                } catch (IOException e) {
                    System.out.println("System HttpURLConnection failed with " + e);
                    return false;
                }
            }
        }

        // GET or POST to one particular URL using Cronet HttpURLConnection API
        private class CronetHttpURLConnectionFetchTask implements Callable<Boolean> {
            private final byte[] mBuffer = new byte[mBufferSize];

            @Override
            public Boolean call() {
                try {
                    return exerciseHttpURLConnection(mCronetEngine.openConnection(mUrl), mBuffer);
                } catch (IOException e) {
                    System.out.println("Cronet HttpURLConnection failed with " + e);
                    return false;
                }
            }
        }

        // GET or POST to one particular URL using Cronet's asynchronous API
        private class CronetAsyncFetchTask implements Callable<Boolean> {
            // A message-queue for asynchronous tasks to post back to.
            private final LinkedBlockingQueue<Runnable> mWorkQueue = new LinkedBlockingQueue<>();
            private final WorkQueueExecutor mWorkQueueExecutor = new WorkQueueExecutor();

            private int mRemainingRequests;
            private int mConcurrentFetchersDone;
            private boolean mFailed;

            CronetAsyncFetchTask() {
                mRemainingRequests = mIterations;
                mConcurrentFetchersDone = 0;
                mFailed = false;
            }

            private void initiateRequest(final ByteBuffer buffer) {
                if (mRemainingRequests == 0) {
                    mConcurrentFetchersDone++;
                    if (mUseNetworkThread) {
                        // Post empty task so message loop exit condition is retested.
                        postToWorkQueue(
                                new Runnable() {
                                    @Override
                                    public void run() {}
                                });
                    }
                    return;
                }
                mRemainingRequests--;
                final Runnable completionCallback =
                        new Runnable() {
                            @Override
                            public void run() {
                                initiateRequest(buffer);
                            }
                        };
                final UrlRequest.Builder builder =
                        mCronetEngine.newUrlRequestBuilder(
                                mUrl.toString(),
                                new Callback(buffer, completionCallback),
                                mWorkQueueExecutor);
                if (mDirection == Direction.UP) {
                    builder.setUploadDataProvider(new Uploader(buffer), mWorkQueueExecutor);
                    builder.addHeader("Content-Type", "application/octet-stream");
                }
                builder.build().start();
            }

            private class Uploader extends UploadDataProvider {
                private final ByteBuffer mBuffer;
                private int mRemainingBytes;

                Uploader(ByteBuffer buffer) {
                    mBuffer = buffer;
                    mRemainingBytes = mLength;
                }

                @Override
                public long getLength() {
                    return mLength;
                }

                @Override
                public void read(UploadDataSink uploadDataSink, ByteBuffer byteBuffer) {
                    mBuffer.clear();
                    // Don't post more than |mLength|.
                    if (mRemainingBytes < mBuffer.limit()) {
                        mBuffer.limit(mRemainingBytes);
                    }
                    // Don't overflow |byteBuffer|.
                    if (byteBuffer.remaining() < mBuffer.limit()) {
                        mBuffer.limit(byteBuffer.remaining());
                    }
                    byteBuffer.put(mBuffer);
                    mRemainingBytes -= mBuffer.position();
                    uploadDataSink.onReadSucceeded(false);
                }

                @Override
                public void rewind(UploadDataSink uploadDataSink) {
                    uploadDataSink.onRewindError(new Exception("no rewinding"));
                }
            }

            private class Callback extends UrlRequest.Callback {
                private final ByteBuffer mBuffer;
                private final Runnable mCompletionCallback;
                private int mBytesReceived;

                Callback(ByteBuffer buffer, Runnable completionCallback) {
                    mBuffer = buffer;
                    mCompletionCallback = completionCallback;
                }

                @Override
                public void onResponseStarted(UrlRequest request, UrlResponseInfo info) {
                    mBuffer.clear();
                    request.read(mBuffer);
                }

                @Override
                public void onRedirectReceived(
                        UrlRequest request, UrlResponseInfo info, String newLocationUrl) {
                    request.followRedirect();
                }

                @Override
                public void onReadCompleted(
                        UrlRequest request, UrlResponseInfo info, ByteBuffer byteBuffer) {
                    mBytesReceived += byteBuffer.position();
                    mBuffer.clear();
                    request.read(mBuffer);
                }

                @Override
                public void onSucceeded(UrlRequest request, UrlResponseInfo info) {
                    if (info.getHttpStatusCode() != 200 || mBytesReceived != mLength) {
                        System.out.println(
                                "Failed: response code: "
                                        + info.getHttpStatusCode()
                                        + " bytes: "
                                        + mBytesReceived);
                        mFailed = true;
                    }
                    mCompletionCallback.run();
                }

                @Override
                public void onFailed(UrlRequest request, UrlResponseInfo info, CronetException e) {
                    System.out.println("Async request failed with " + e);
                    mFailed = true;
                }
            }

            private void postToWorkQueue(Runnable task) {
                try {
                    mWorkQueue.put(task);
                } catch (InterruptedException e) {
                    mFailed = true;
                }
            }

            private class WorkQueueExecutor implements Executor {
                @Override
                public void execute(Runnable task) {
                    if (mUseNetworkThread) {
                        task.run();
                    } else {
                        postToWorkQueue(task);
                    }
                }
            }

            @Override
            public Boolean call() {
                // Initiate concurrent requests.
                for (int i = 0; i < mConcurrency; i++) {
                    initiateRequest(ByteBuffer.allocateDirect(mBufferSize));
                }
                // Wait for all jobs to finish.
                try {
                    while (mConcurrentFetchersDone != mConcurrency && !mFailed) {
                        mWorkQueue.take().run();
                    }
                } catch (InterruptedException e) {
                    System.out.println("Async tasks failed with " + e);
                    mFailed = true;
                }
                return !mFailed;
            }
        }

        /** Executes the benchmark, times how long it takes, and records time in |mResults|. */
        public void run() {
            final ExecutorService executor = Executors.newFixedThreadPool(mConcurrency);
            final List<Callable<Boolean>> tasks = new ArrayList<>(mIterations);
            startLogging();
            // Prepare list of tasks to run.
            switch (mMode) {
                case SYSTEM_HUC:
                    for (int i = 0; i < mIterations; i++) {
                        tasks.add(new SystemHttpURLConnectionFetchTask());
                    }
                    break;
                case CRONET_HUC:
                    {
                        for (int i = 0; i < mIterations; i++) {
                            tasks.add(new CronetHttpURLConnectionFetchTask());
                        }
                        break;
                    }
                case CRONET_ASYNC:
                    tasks.add(new CronetAsyncFetchTask());
                    break;
                default:
                    throw new IllegalArgumentException("Unknown mode: " + mMode);
            }
            // Execute tasks.
            boolean success = true;
            List<Future<Boolean>> futures = new ArrayList<>();
            try {
                startTimer();
                // If possible execute directly to lessen impact of thread-pool overhead.
                if (tasks.size() == 1 || mConcurrency == 1) {
                    for (int i = 0; i < tasks.size(); i++) {
                        if (!tasks.get(i).call()) {
                            success = false;
                        }
                    }
                } else {
                    futures = executor.invokeAll(tasks);
                    executor.shutdown();
                    executor.awaitTermination(240, TimeUnit.SECONDS);
                }
                stopTimer();
                for (Future<Boolean> future : futures) {
                    if (!future.isDone() || !future.get()) {
                        success = false;
                        break;
                    }
                }
            } catch (Exception e) {
                System.out.println("Batch execution failed with " + e);
                success = false;
            }
            stopLogging();
            if (success) {
                reportResult();
            }
        }
    }

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        // Initializing application context here due to lack of custom CronetPerfTestApplication.
        ContextUtils.initApplicationContext(getApplicationContext());
        PathUtils.setPrivateDataDirectorySuffix(PRIVATE_DATA_DIRECTORY_SUFFIX);
        mConfig = getIntent().getData();
        // Execute benchmarks on another thread to avoid networking on main thread.

        PostTask.postTask(
                TaskTraits.USER_BLOCKING,
                () -> {
                    JSONObject results = new JSONObject();
                    for (Mode mode : Mode.values()) {
                        for (Direction direction : Direction.values()) {
                            for (Protocol protocol : Protocol.values()) {
                                if (protocol == Protocol.QUIC && mode == Mode.SYSTEM_HUC) {
                                    // Unsupported; skip.
                                    continue;
                                }
                                // Run large and small benchmarks one at a time to test
                                // single-threaded use.
                                // Also run them four at a time to see how they benefit from
                                // concurrency.
                                // The value four was chosen as many devices are now quad-core.
                                new Benchmark(mode, direction, Size.LARGE, protocol, 1, results)
                                        .run();
                                new Benchmark(mode, direction, Size.LARGE, protocol, 4, results)
                                        .run();
                                new Benchmark(mode, direction, Size.SMALL, protocol, 1, results)
                                        .run();
                                new Benchmark(mode, direction, Size.SMALL, protocol, 4, results)
                                        .run();
                                // Large benchmarks are generally bandwidth bound and unaffected by
                                // per-request overhead.  Small benchmarks are not, so test at
                                // further increased concurrency to see if further benefit is
                                // possible.
                                new Benchmark(mode, direction, Size.SMALL, protocol, 8, results)
                                        .run();
                            }
                        }
                    }
                    final File outputFile = new File(getConfigString("RESULTS_FILE"));
                    final File doneFile = new File(getConfigString("DONE_FILE"));
                    // If DONE_FILE exists, something is horribly wrong, produce no results to
                    // convey this.
                    if (doneFile.exists()) {
                        results = new JSONObject();
                    }
                    // Write out results to RESULTS_FILE, then create DONE_FILE.
                    FileOutputStream outputFileStream = null;
                    FileOutputStream doneFileStream = null;
                    try {
                        outputFileStream = new FileOutputStream(outputFile);
                        outputFileStream.write(results.toString().getBytes());
                        outputFileStream.close();
                        doneFileStream = new FileOutputStream(doneFile);
                        doneFileStream.close();
                    } catch (Exception e) {
                        System.out.println("Failed write results file: " + e);
                    } finally {
                        try {
                            if (outputFileStream != null) {
                                outputFileStream.close();
                            }
                            if (doneFileStream != null) {
                                doneFileStream.close();
                            }
                        } catch (IOException e) {
                            System.out.println("Failed to close output file: " + e);
                        }
                    }
                    finish();
                });
    }
}
