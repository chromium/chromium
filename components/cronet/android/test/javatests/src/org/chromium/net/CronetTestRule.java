// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net;

import static com.google.common.truth.Truth.assertThat;

import static org.junit.Assume.assumeTrue;

import android.content.Context;
import android.content.MutableContextWrapper;
import android.os.Build;
import android.os.StrictMode;

import androidx.test.core.app.ApplicationProvider;

import org.junit.rules.TestRule;
import org.junit.runner.Description;
import org.junit.runners.model.Statement;

import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.PathUtils;
import org.chromium.net.impl.JavaCronetProvider;
import org.chromium.net.impl.NativeCronetProvider;
import org.chromium.net.impl.UserAgent;

import java.io.File;
import java.lang.annotation.Annotation;
import java.lang.annotation.ElementType;
import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.lang.annotation.Target;

/**
 * Custom TestRule for Cronet instrumentation tests.
 */
public class CronetTestRule implements TestRule {
    private static final String PRIVATE_DATA_DIRECTORY_SUFFIX = "cronet_test";
    private static final String TAG = "CronetTestRule";

    private CronetTestFramework mCronetTestFramework;
    private CronetImplementation mImplementation;

    private final EngineStartupMode mEngineStartupMode;

    private CronetTestRule(EngineStartupMode engineStartupMode) {
        this.mEngineStartupMode = engineStartupMode;
    }

    /**
     * Starts the Cronet engine automatically for each test case, but doesn't allow any
     * customizations to the builder.
     */
    public static CronetTestRule withManualEngineStartup() {
        return new CronetTestRule(EngineStartupMode.MANUAL);
    }

    /**
     * Requires the user to call {@code CronetTestFramework.startEngine()} but allows to customize
     * the builder parameters.
     */
    public static CronetTestRule withAutomaticEngineStartup() {
        return new CronetTestRule(EngineStartupMode.AUTOMATIC);
    }

    public CronetTestFramework getTestFramework() {
        return mCronetTestFramework;
    }

    public void assertResponseEquals(UrlResponseInfo expected, UrlResponseInfo actual) {
        assertThat(actual.getAllHeaders()).isEqualTo(expected.getAllHeaders());
        assertThat(actual.getAllHeadersAsList()).isEqualTo(expected.getAllHeadersAsList());
        assertThat(actual.getHttpStatusCode()).isEqualTo(expected.getHttpStatusCode());
        assertThat(actual.getHttpStatusText()).isEqualTo(expected.getHttpStatusText());
        assertThat(actual.getUrlChain()).isEqualTo(expected.getUrlChain());
        assertThat(actual.getUrl()).isEqualTo(expected.getUrl());
        // Transferred bytes and proxy server are not supported in pure java
        if (!testingJavaImpl()) {
            assertThat(actual.getReceivedByteCount()).isEqualTo(expected.getReceivedByteCount());
            assertThat(actual.getProxyServer()).isEqualTo(expected.getProxyServer());
            // This is a place where behavior intentionally differs between native and java
            assertThat(actual.getNegotiatedProtocol()).isEqualTo(expected.getNegotiatedProtocol());
        }
    }

    /**
     * Returns {@code true} when test is being run against the java implementation of CronetEngine.
     *
     * @deprecated use the implementation enum
     */
    @Deprecated
    public boolean testingJavaImpl() {
        return mImplementation.equals(CronetImplementation.FALLBACK);
    }

    @Override
    public Statement apply(final Statement base, final Description desc) {
        return new Statement() {
            @Override
            public void evaluate() throws Throwable {
                runBase(base, desc);
            }
        };
    }

    // TODO(yolandyan): refactor this using parameterize framework
    private void runBase(Statement base, Description desc) throws Throwable {
        setImplementationUnderTest(CronetImplementation.STATICALLY_LINKED);
        String packageName = desc.getTestClass().getPackage().getName();

        boolean onlyRunTestForNative = desc.getAnnotation(OnlyRunNativeCronet.class) != null
                || desc.getTestClass().getAnnotation(OnlyRunNativeCronet.class) != null;
        boolean onlyRunTestForJava = desc.getAnnotation(OnlyRunJavaCronet.class) != null;
        if (onlyRunTestForNative && onlyRunTestForJava) {
            throw new IllegalArgumentException(desc.getMethodName()
                    + " skipped because it specified both "
                    + "OnlyRunNativeCronet and OnlyRunJavaCronet annotations");
        }
        boolean doRunTestForNative = onlyRunTestForNative || !onlyRunTestForJava;
        boolean doRunTestForJava = onlyRunTestForJava || !onlyRunTestForNative;

        // Find the API version required by the test.
        int requiredApiVersion = getMaximumAvailableApiLevel();
        int requiredAndroidApiVersion = Build.VERSION_CODES.KITKAT;
        for (Annotation a : desc.getTestClass().getAnnotations()) {
            if (a instanceof RequiresMinApi) {
                requiredApiVersion = ((RequiresMinApi) a).value();
            }
            if (a instanceof RequiresMinAndroidApi) {
                requiredAndroidApiVersion = ((RequiresMinAndroidApi) a).value();
            }
        }
        for (Annotation a : desc.getAnnotations()) {
            // Method scoped requirements take precedence over class scoped
            // requirements.
            if (a instanceof RequiresMinApi) {
                requiredApiVersion = ((RequiresMinApi) a).value();
            }
            if (a instanceof RequiresMinAndroidApi) {
                requiredAndroidApiVersion = ((RequiresMinAndroidApi) a).value();
            }
        }
        assumeTrue(desc.getMethodName() + " skipped because it requires API " + requiredApiVersion
                        + " but only API " + getMaximumAvailableApiLevel() + " is present.",
                getMaximumAvailableApiLevel() >= requiredApiVersion);
        assumeTrue(desc.getMethodName() + " skipped because it Android's API level "
                        + requiredAndroidApiVersion + " but test device supports only API "
                        + Build.VERSION.SDK_INT,
                Build.VERSION.SDK_INT >= requiredAndroidApiVersion);

        if (packageName.startsWith("org.chromium.net")) {
            try {
                if (doRunTestForNative) {
                    Log.i(TAG, "Running test against Native implementation.");
                    evaluateWithFramework(base);
                }
                if (doRunTestForJava) {
                    Log.i(TAG, "Running test against Java implementation.");
                    setImplementationUnderTest(CronetImplementation.FALLBACK);
                    evaluateWithFramework(base);
                }
            } catch (Throwable e) {
                Log.e(TAG, "CronetTestBase#runTest failed for %s implementation.", mImplementation);
                throw e;
            }
        } else {
            evaluateWithFramework(base);
        }
    }

    private void evaluateWithFramework(Statement statement) throws Throwable {
        try (CronetTestFramework framework = createCronetTestFramework()) {
            statement.evaluate();
        } finally {
            mCronetTestFramework = null;
        }
    }

    private CronetTestFramework createCronetTestFramework() {
        mCronetTestFramework = new CronetTestFramework(mImplementation);
        if (mEngineStartupMode.equals(EngineStartupMode.AUTOMATIC)) {
            mCronetTestFramework.startEngine();
        }
        return mCronetTestFramework;
    }

    static int getMaximumAvailableApiLevel() {
        // Prior to M59 the ApiVersion.getMaximumAvailableApiLevel API didn't exist
        int cronetMajorVersion = Integer.parseInt(ApiVersion.getCronetVersion().split("\\.")[0]);
        if (cronetMajorVersion < 59) {
            return 3;
        }
        return ApiVersion.getMaximumAvailableApiLevel();
    }

    /**
     * Annotation for test classes or methods in org.chromium.net package that disables rerunning
     * the test against the Java-only implementation. When this annotation is present the test is
     * only run against the native implementation.
     */
    @Target({ElementType.TYPE, ElementType.METHOD})
    @Retention(RetentionPolicy.RUNTIME)
    public @interface OnlyRunNativeCronet {}

    /**
     * Annotation for test methods in org.chromium.net package that disables rerunning the test
     * against the Native/Chromium implementation. When this annotation is present the test is only
     * run against the Java implementation.
     */
    @Target(ElementType.METHOD)
    @Retention(RetentionPolicy.RUNTIME)
    public @interface OnlyRunJavaCronet {}

    /**
     * Annotation allowing classes or individual tests to be skipped based on the version of the
     * Cronet API present. Takes the minimum API version upon which the test should be run.
     * For example if a test should only be run with API version 2 or greater:
     *   @RequiresMinApi(2)
     *   public void testFoo() {}
     */
    @Target({ElementType.TYPE, ElementType.METHOD})
    @Retention(RetentionPolicy.RUNTIME)
    public @interface RequiresMinApi {
        int value();
    }

    /**
     * Annotation allowing classes or individual tests to be skipped based on the Android OS version
     * installed in the deviced used for testing. Takes the minimum API version upon which the test
     * should be run. For example if a test should only be run with Android Oreo or greater:
     *   @RequiresMinApi(Build.VERSION_CODES.O)
     *   public void testFoo() {}
     */
    @Target({ElementType.TYPE, ElementType.METHOD})
    @Retention(RetentionPolicy.RUNTIME)
    public @interface RequiresMinAndroidApi {
        int value();
    }

    /**
     * Prepares the path for the test storage (http cache, QUIC server info).
     */
    public static void prepareTestStorage(Context context) {
        File storage = new File(getTestStorageDirectory());
        if (storage.exists()) {
            assertThat(recursiveDelete(storage)).isTrue();
        }
        ensureTestStorageExists();
    }

    /**
     * Returns the path for the test storage (http cache, QUIC server info).
     * Also ensures it exists.
     */
    public static String getTestStorage(Context context) {
        ensureTestStorageExists();
        return getTestStorageDirectory();
    }

    /**
     * Returns the path for the test storage (http cache, QUIC server info).
     * NOTE: Does not ensure it exists; tests should use {@link #getTestStorage}.
     */
    private static String getTestStorageDirectory() {
        return PathUtils.getDataDirectory() + "/test_storage";
    }

    /**
     * Ensures test storage directory exists, i.e. creates one if it does not exist.
     */
    private static void ensureTestStorageExists() {
        File storage = new File(getTestStorageDirectory());
        if (!storage.exists()) {
            assertThat(storage.mkdir()).isTrue();
        }
    }

    private static boolean recursiveDelete(File path) {
        if (path.isDirectory()) {
            for (File c : path.listFiles()) {
                if (!recursiveDelete(c)) {
                    return false;
                }
            }
        }
        return path.delete();
    }

    private void setImplementationUnderTest(CronetImplementation implementation) {
        mImplementation = implementation;
    }

    /**
     * Creates and holds pointer to CronetEngine.
     */
    public static class CronetTestFramework implements AutoCloseable {
        private final CronetImplementation mImplementation;
        private final ExperimentalCronetEngine.Builder mBuilder;
        private final MutableContextWrapper mContextWrapper;
        private final StrictMode.VmPolicy mOldVmPolicy;

        private ExperimentalCronetEngine mCronetEngine;
        private boolean mClosed;

        private CronetTestFramework(CronetImplementation implementation) {
            this.mContextWrapper =
                    new MutableContextWrapper(ApplicationProvider.getApplicationContext());
            this.mBuilder = implementation.createBuilder(mContextWrapper)
                                    .setUserAgent(UserAgent.from(mContextWrapper))
                                    .enableQuic(true);
            this.mImplementation = implementation;

            System.loadLibrary("cronet_tests");
            ContextUtils.initApplicationContext(getContext().getApplicationContext());
            PathUtils.setPrivateDataDirectorySuffix(PRIVATE_DATA_DIRECTORY_SUFFIX);
            prepareTestStorage(getContext());
            mOldVmPolicy = StrictMode.getVmPolicy();
            // Only enable StrictMode testing after leaks were fixed in crrev.com/475945
            if (getMaximumAvailableApiLevel() >= 7) {
                StrictMode.setVmPolicy(new StrictMode.VmPolicy.Builder()
                                               .detectLeakedClosableObjects()
                                               .penaltyLog()
                                               .penaltyDeath()
                                               .build());
            }
        }

        /**
         * Replaces the {@link Context} implementation that the Cronet engine calls into. Useful for
         * faking/mocking Android context calls.
         *
         * @throws IllegalStateException if called after the Cronet engine has already been built.
         * Intercepting context calls while the code under test is running is racy and runs the risk
         * that the code under test will not pick up the change.
         */
        public void interceptContext(ContextInterceptor contextInterceptor) {
            checkNotClosed();

            if (mCronetEngine != null) {
                throw new IllegalStateException(
                        "Refusing to intercept context after the Cronet engine has been built");
            }

            mContextWrapper.setBaseContext(
                    contextInterceptor.interceptContext(mContextWrapper.getBaseContext()));
        }

        /**
         * @return the context to be used by the Cronet engine
         *
         * @see #interceptContext
         */
        public Context getContext() {
            checkNotClosed();
            return mContextWrapper;
        }

        public CronetEngine.Builder enableDiskCache(CronetEngine.Builder cronetEngineBuilder) {
            cronetEngineBuilder.setStoragePath(getTestStorage(getContext()));
            cronetEngineBuilder.enableHttpCache(CronetEngine.Builder.HTTP_CACHE_DISK, 1000 * 1024);
            return cronetEngineBuilder;
        }

        public ExperimentalCronetEngine startEngine() {
            checkNotClosed();

            if (mCronetEngine != null) {
                throw new IllegalStateException("Engine is already started!");
            }

            mCronetEngine = mBuilder.build();
            mImplementation.verifyCronetEngineInstance(mCronetEngine);

            // Start collecting metrics.
            mCronetEngine.getGlobalMetricsDeltas();

            return mCronetEngine;
        }

        public ExperimentalCronetEngine getEngine() {
            checkNotClosed();

            if (mCronetEngine == null) {
                throw new IllegalStateException("Engine not started yet!");
            }

            return mCronetEngine;
        }

        /**
         * Applies the given patch to the primary Cronet Engine builder associated with this run.
         */
        public void applyEngineBuilderPatch(CronetBuilderPatch patch) {
            checkNotClosed();

            if (mCronetEngine != null) {
                throw new IllegalStateException("The engine was already built!");
            }

            try {
                patch.apply(mBuilder);
            } catch (Exception e) {
                throw new IllegalArgumentException("Cannot apply the given patch!", e);
            }
        }

        /**
         * Returns a new instance of a Cronet builder corresponding to the implementation under
         * test.
         *
         * <p>Some test cases need to create multiple instances of Cronet engines to test
         * interactions between them, so we provide the capability to do so and reliably obtain
         * the correct Cronet implementation.
         *
         * <p>Note that this builder and derived Cronet engine is not managed by the framework! The
         * caller is responsible for cleaning up resources (e.g. calling {@code engine.shutdown()}
         * at the end of the test).
         *
         */
        public ExperimentalCronetEngine.Builder createNewSecondaryBuilder(Context context) {
            return mImplementation.createBuilder(context);
        }

        @Override
        public void close() {
            if (mClosed) {
                return;
            }
            shutdownEngine();
            mClosed = true;

            try {
                // Run GC and finalizers a few times to pick up leaked closeables
                for (int i = 0; i < 10; i++) {
                    System.gc();
                    System.runFinalization();
                }
            } finally {
                StrictMode.setVmPolicy(mOldVmPolicy);
            }
        }

        private void shutdownEngine() {
            if (mCronetEngine == null) {
                return;
            }
            try {
                mCronetEngine.shutdown();
            } catch (IllegalStateException e) {
                if (e.getMessage().contains("Engine is shut down")) {
                    // We're trying to shut the engine down repeatedly. Make such calls idempotent
                    // instead of failing, as there's no API to query whether an engine is shut down
                    // and some tests shut the engine down deliberately (e.g. to make sure
                    // everything is flushed properly).
                    Log.d(TAG, "Cronet engine already shut down by the test.", e);
                } else {
                    throw e;
                }
            }
            mCronetEngine = null;
        }

        private void checkNotClosed() {
            if (mClosed) {
                throw new IllegalStateException(
                        "Unable to interact with a closed CronetTestFramework!");
            }
        }
    }

    /**
     * A functional interface that allows Cronet tests to modify parameters of the Cronet engine
     * provided by {@code CronetTestFramework}.
     *
     * <p>The builder itself isn't exposed directly as a getter to tests to stress out ownership
     * and make accidental local access less likely.
     */
    public static interface CronetBuilderPatch {
        public void apply(ExperimentalCronetEngine.Builder builder) throws Exception;
    }

    private enum EngineStartupMode {
        MANUAL,
        AUTOMATIC,
    }

    // This is a replacement for java.util.function.Function as Function is only available
    // starting android API level 24.
    private interface EngineBuilderSupplier {
        ExperimentalCronetEngine.Builder getCronetEngineBuilder(Context context);
    }

    public enum CronetImplementation {
        STATICALLY_LINKED(context
                -> (ExperimentalCronetEngine.Builder) new NativeCronetProvider(context)
                           .createBuilder()),
        FALLBACK((context)
                         -> (ExperimentalCronetEngine.Builder) new JavaCronetProvider(context)
                                    .createBuilder()),
        AOSP_PLATFORM(
                (context) -> { throw new UnsupportedOperationException("Not implemented yet"); });

        private final EngineBuilderSupplier mEngineSupplier;

        private CronetImplementation(EngineBuilderSupplier engineSupplier) {
            this.mEngineSupplier = engineSupplier;
        }

        ExperimentalCronetEngine.Builder createBuilder(Context context) {
            return mEngineSupplier.getCronetEngineBuilder(context);
        }

        private void verifyCronetEngineInstance(CronetEngine engine) {
            // TODO(danstahr): Add assertions for expected class
        }
    }
}
