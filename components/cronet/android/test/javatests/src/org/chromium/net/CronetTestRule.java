// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net;

import static com.google.common.truth.Truth.assertThat;
import static com.google.common.truth.Truth.assertWithMessage;

import static org.junit.Assume.assumeFalse;
import static org.junit.Assume.assumeTrue;

import static org.chromium.net.truth.UrlResponseInfoSubject.assertThat;

import android.content.Context;
import android.content.MutableContextWrapper;
import android.os.Build;
import android.os.StrictMode;

import androidx.annotation.Nullable;
import androidx.test.core.app.ApplicationProvider;

import org.junit.rules.TestRule;
import org.junit.runner.Description;
import org.junit.runners.model.Statement;

import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.PathUtils;
import org.chromium.net.httpflags.Flags;
import org.chromium.net.httpflags.HttpFlagsInterceptor;
import org.chromium.net.impl.CronetManifest;
import org.chromium.net.impl.CronetUrlRequestContext;
import org.chromium.net.impl.HttpEngineNativeProvider;
import org.chromium.net.impl.JavaCronetEngine;
import org.chromium.net.impl.JavaCronetProvider;
import org.chromium.net.impl.NativeCronetProvider;
import org.chromium.net.impl.UserAgent;
import org.chromium.net.impl.VersionSafeCallbacks;

import java.io.File;
import java.lang.annotation.Annotation;
import java.lang.annotation.ElementType;
import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.lang.annotation.Target;
import java.util.Arrays;
import java.util.EnumSet;
import java.util.Set;

/** Custom TestRule for Cronet instrumentation tests. */
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
     * Requires the user to call {@code CronetTestFramework.startEngine()} but allows to customize
     * the builder parameters.
     */
    public static CronetTestRule withManualEngineStartup() {
        return new CronetTestRule(EngineStartupMode.MANUAL);
    }

    /**
     * Starts the Cronet engine automatically for each test case, but doesn't allow any
     * customizations to the builder.
     */
    public static CronetTestRule withAutomaticEngineStartup() {
        return new CronetTestRule(EngineStartupMode.AUTOMATIC);
    }

    public CronetTestFramework getTestFramework() {
        return mCronetTestFramework;
    }

    public void assertResponseEquals(UrlResponseInfo expected, UrlResponseInfo actual) {
        assertThat(actual).hasHeadersThat().isEqualTo(expected.getAllHeaders());
        assertThat(actual).hasHeadersListThat().isEqualTo(expected.getAllHeadersAsList());
        assertThat(actual).hasHttpStatusCodeThat().isEqualTo(expected.getHttpStatusCode());
        assertThat(actual).hasHttpStatusTextThat().isEqualTo(expected.getHttpStatusText());
        assertThat(actual).hasUrlChainThat().isEqualTo(expected.getUrlChain());
        assertThat(actual).hasUrlThat().isEqualTo(expected.getUrl());
        // Transferred bytes and proxy server are not supported in pure java
        if (!testingJavaImpl()) {
            assertThat(actual)
                    .hasReceivedByteCountThat()
                    .isEqualTo(expected.getReceivedByteCount());
            assertThat(actual).hasProxyServerThat().isEqualTo(expected.getProxyServer());
            // This is a place where behavior intentionally differs between native and java
            assertThat(actual)
                    .hasNegotiatedProtocolThat()
                    .isEqualTo(expected.getNegotiatedProtocol());
        }
    }

    public void assertCronetInternalErrorCode(NetworkException exception, int expectedErrorCode) {
        switch (implementationUnderTest()) {
            case STATICALLY_LINKED:
                assertThat(exception.getCronetInternalErrorCode()).isEqualTo(expectedErrorCode);
                break;
            case AOSP_PLATFORM:
            case FALLBACK:
                // Internal error codes aren't supported in the fallback implementation, and
                // inaccessible in AOSP
                break;
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

    public CronetImplementation implementationUnderTest() {
        return mImplementation;
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
        String testName = desc.getTestClass().getName() + "#" + desc.getMethodName();

        // Find the API version required by the test.
        int requiredApiVersion = getMaximumAvailableApiLevel();
        int requiredAndroidApiVersion = Build.VERSION_CODES.M;
        boolean netLogEnabled = true;
        for (Annotation a : desc.getTestClass().getAnnotations()) {
            if (a instanceof RequiresMinApi) {
                requiredApiVersion = ((RequiresMinApi) a).value();
            }
            if (a instanceof RequiresMinAndroidApi) {
                requiredAndroidApiVersion = ((RequiresMinAndroidApi) a).value();
            }
            if (a instanceof DisableAutomaticNetLog) {
                netLogEnabled = false;
                Log.i(
                        TAG,
                        "Disabling automatic NetLog collection due to: "
                                + ((DisableAutomaticNetLog) a).reason());
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
            if (a instanceof DisableAutomaticNetLog) {
                netLogEnabled = false;
                Log.i(
                        TAG,
                        "Disabling automatic NetLog collection due to: "
                                + ((DisableAutomaticNetLog) a).reason());
            }
        }

        assumeTrue(
                desc.getMethodName()
                        + " skipped because it requires API "
                        + requiredApiVersion
                        + " but only API "
                        + getMaximumAvailableApiLevel()
                        + " is present.",
                getMaximumAvailableApiLevel() >= requiredApiVersion);
        assumeTrue(
                desc.getMethodName()
                        + " skipped because it Android's API level "
                        + requiredAndroidApiVersion
                        + " but test device supports only API "
                        + Build.VERSION.SDK_INT,
                Build.VERSION.SDK_INT >= requiredAndroidApiVersion);

        EnumSet<CronetImplementation> excludedImplementations =
                EnumSet.noneOf(CronetImplementation.class);
        IgnoreFor ignoreDueToClassAnnotation = getTestClassAnnotation(desc, IgnoreFor.class);
        if (ignoreDueToClassAnnotation != null) {
            excludedImplementations.addAll(
                    Arrays.asList(ignoreDueToClassAnnotation.implementations()));
        }
        IgnoreFor ignoreDueToMethodAnnotation = getTestMethodAnnotation(desc, IgnoreFor.class);
        if (ignoreDueToMethodAnnotation != null) {
            excludedImplementations.addAll(
                    Arrays.asList(ignoreDueToMethodAnnotation.implementations()));
        }

        Set<CronetImplementation> implementationsUnderTest =
                EnumSet.complementOf(excludedImplementations);
        assertWithMessage(
                        "Test should not be skipped via IgnoreFor annotation. "
                                + "Use DisabledTest instead")
                .that(implementationsUnderTest)
                .isNotEmpty();

        if (Build.VERSION.SDK_INT < 34) {
            implementationsUnderTest.remove(CronetImplementation.AOSP_PLATFORM);
            assumeFalse(
                    desc.getMethodName()
                            + " skipped because it's supposed to run against only AOSP_PLATFORM but"
                            + " test device is not U+",
                    implementationsUnderTest.isEmpty());
        }

        Log.i(TAG, "Implementations to be tested against: %s", implementationsUnderTest);

        if (packageName.startsWith("org.chromium.net")) {
            for (CronetImplementation implementation : implementationsUnderTest) {
                if (isRunningInAOSP() && implementation.equals(CronetImplementation.FALLBACK)) {
                    // Skip executing tests for JavaCronetEngine.
                    continue;
                }
                Log.i(TAG, "Running test against " + implementation + " implementation.");
                setImplementationUnderTest(implementation);
                evaluateWithFramework(base, testName, netLogEnabled);
            }
        } else {
            evaluateWithFramework(base, testName, netLogEnabled);
        }
    }

    /**
     * This method only returns the value of the `is_running_in_aosp` flag which for Chromium can be
     * found inside components/cronet/android/test/res/values/cronet-test-rule-configuration.xml
     * for which it should be equal to false. However, on AOSP, we ship a different value
     * which is equal to true.
     *
     * <p>This distinction between where the tests are being executed is crucial because we don't
     * want to run JavaCronetEngine tests in AOSP.
     *
     * @return True if the tests are being executed in AOSP.
     */
    @SuppressWarnings("DiscouragedApi")
    public boolean isRunningInAOSP() {
        int resId =
                ApplicationProvider.getApplicationContext()
                        .getResources()
                        .getIdentifier(
                                "is_running_in_aosp",
                                "bool",
                                ApplicationProvider.getApplicationContext().getPackageName());
        if (resId == 0) {
            throw new IllegalStateException(
                    "Could not find any value for `is_running_in_aosp` boolean entry.");
        }
        return ApplicationProvider.getApplicationContext().getResources().getBoolean(resId);
    }

    private void evaluateWithFramework(Statement statement, String testName, boolean netLogEnabled)
            throws Throwable {
        try (CronetTestFramework framework = createCronetTestFramework(testName, netLogEnabled)) {
            statement.evaluate();
        } finally {
            mCronetTestFramework = null;
        }
    }

    private CronetTestFramework createCronetTestFramework(String testName, boolean netLogEnabled) {
        mCronetTestFramework = new CronetTestFramework(mImplementation, testName, netLogEnabled);
        if (mEngineStartupMode.equals(EngineStartupMode.AUTOMATIC)) {
            mCronetTestFramework.startEngine();
        }
        return mCronetTestFramework;
    }

    static int getMaximumAvailableApiLevel() {
        return VersionSafeCallbacks.ApiVersion.getMaximumAvailableApiLevel();
    }

    /**
     * Annotation allowing classes or individual tests to be skipped based on the implementation
     * being currently tested. When this annotation is present the test is only run against the
     * {@link CronetImplementation} cases not specified in the annotation. If the annotation is
     * specified both at the class and method levels, the union of IgnoreFor#implementations() will
     * be skipped.
     */
    @Target({ElementType.TYPE, ElementType.METHOD})
    @Retention(RetentionPolicy.RUNTIME)
    public @interface IgnoreFor {
        CronetImplementation[] implementations();

        String reason();
    }

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

    /** Annotation allowing classes or individual tests to disable automatic NetLog collection. */
    @Target({ElementType.TYPE, ElementType.METHOD})
    @Retention(RetentionPolicy.RUNTIME)
    public @interface DisableAutomaticNetLog {
        String reason();
    }

    /** Prepares the path for the test storage (http cache, QUIC server info). */
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

    /** Ensures test storage directory exists, i.e. creates one if it does not exist. */
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

    /** Creates and holds pointer to CronetEngine. */
    public static class CronetTestFramework implements AutoCloseable {
        // This is the Context that Cronet will use. The specific Context instance can never change
        // because that would break ContextUtils.initApplicationContext(). We work around this by
        // using a static MutableContextWrapper whose identity is constant, but the wrapped
        // Context isn't.
        //
        // TODO: in theory, no code under test should be running in between tests, and we should be
        // able to enforce that by rejecting all Context calls in between tests (e.g. by resetting
        // the base context to null while not running a test). Unfortunately, it's not that simple
        // because the code under test doesn't currently wait for all asynchronous operations to
        // complete before the test finishes (e.g. ProxyChangeListener can call back into the
        // CronetInit thread even while a test isn't running), so we have to keep that context
        // working even in between tests to prevent crashes. This is problematic as that makes tests
        // non-hermetic/racy/brittle. Ideally, we should ensure that no code under test can run in
        // between tests.
        @SuppressWarnings("StaticFieldLeak")
        private static final MutableContextWrapper sContextWrapper =
                new MutableContextWrapper(ApplicationProvider.getApplicationContext()) {
                    @Override
                    public Context getApplicationContext() {
                        // Ensure the code under test (in particular, the CronetEngineBuilderImpl
                        // constructor) cannot use this method to "escape" context interception.
                        return this;
                    }
                };

        private final CronetImplementation mImplementation;
        private final ExperimentalCronetEngine.Builder mBuilder;
        private final MutableContextWrapper mContextWrapperWithoutFlags;
        private final MutableContextWrapper mContextWrapper;
        private final StrictMode.VmPolicy mOldVmPolicy;
        private final String mTestName;
        private final boolean mNetLogEnabled;

        private HttpFlagsInterceptor mHttpFlagsInterceptor;
        private ExperimentalCronetEngine mCronetEngine;
        private boolean mClosed;

        private CronetTestFramework(
                CronetImplementation implementation, String testName, boolean netLogEnabled) {
            mContextWrapperWithoutFlags =
                    new MutableContextWrapper(ApplicationProvider.getApplicationContext());
            mContextWrapper = new MutableContextWrapper(mContextWrapperWithoutFlags);
            assert sContextWrapper.getBaseContext() == ApplicationProvider.getApplicationContext();
            sContextWrapper.setBaseContext(mContextWrapper);
            mBuilder =
                    implementation
                            .createBuilder(sContextWrapper)
                            .setUserAgent(UserAgent.from(sContextWrapper))
                            .enableQuic(true);
            mImplementation = implementation;
            mTestName = testName;
            mNetLogEnabled = netLogEnabled;

            System.loadLibrary("cronet_tests");
            ContextUtils.initApplicationContext(sContextWrapper);
            PathUtils.setPrivateDataDirectorySuffix(PRIVATE_DATA_DIRECTORY_SUFFIX);
            prepareTestStorage(getContext());
            mOldVmPolicy = StrictMode.getVmPolicy();
            // Only enable StrictMode testing after leaks were fixed in crrev.com/475945
            if (getMaximumAvailableApiLevel() >= 7) {
                StrictMode.setVmPolicy(
                        new StrictMode.VmPolicy.Builder()
                                .detectLeakedClosableObjects()
                                .penaltyLog()
                                .penaltyDeath()
                                .build());
            }

            CronetManifest.resetCache();
            setHttpFlags(null);
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

            mContextWrapperWithoutFlags.setBaseContext(
                    contextInterceptor.interceptContext(
                            mContextWrapperWithoutFlags.getBaseContext()));
        }

        /**
         * Sets the HTTP flags, if any, that the code under test should run with. This affects the
         * behavior of the {@link Context} that the code under test sees.
         *
         * If this method is never called, the default behavior is to simulate the absence of a
         * flags file. This ensures that the code under test does not end up accidentally using a
         * flags file from the host system, which would lead to non-deterministic results.
         *
         * @param flagsFileContents the contents of the flags file, or null to simulate a missing
         * file (default behavior).
         *
         * @throws IllegalStateException if called after the engine has already been built.
         * Modifying flags while the code under test is running is always a mistake, because the
         * code under test won't notice the changes.
         *
         * @see org.chromium.net.impl.HttpFlagsLoader
         * @see HttpFlagsInterceptor
         */
        public void setHttpFlags(@Nullable Flags flagsFileContents) {
            checkNotClosed();

            if (mCronetEngine != null) {
                throw new IllegalStateException(
                        "Refusing to replace flags file provider after the Cronet engine has been "
                                + "built");
            }

            if (mHttpFlagsInterceptor != null) mHttpFlagsInterceptor.close();
            mHttpFlagsInterceptor = new HttpFlagsInterceptor(flagsFileContents);
            mContextWrapper.setBaseContext(
                    mHttpFlagsInterceptor.interceptContext(mContextWrapperWithoutFlags));
        }

        /**
         * @return the context to be used by the Cronet engine
         *
         * @see #interceptContext
         * @see #setFlagsFileContents
         */
        public Context getContext() {
            checkNotClosed();
            return sContextWrapper;
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

            if (mNetLogEnabled) {
                File dataDir = new File(PathUtils.getDataDirectory());
                File netLogDir = new File(dataDir, "NetLog");
                netLogDir.mkdir();
                String netLogFileName =
                        mTestName + "-" + String.valueOf(System.currentTimeMillis());
                File netLogFile = new File(netLogDir, netLogFileName + ".json");
                Log.i(TAG, "Enabling netlog to: " + netLogFile.getPath());
                mCronetEngine.startNetLogToFile(netLogFile.getPath(), /* logAll= */ true);
            }

            return mCronetEngine;
        }

        public ExperimentalCronetEngine getEngine() {
            checkNotClosed();

            if (mCronetEngine == null) {
                throw new IllegalStateException("Engine not started yet!");
            }

            return mCronetEngine;
        }

        /** Applies the given patch to the primary Cronet Engine builder associated with this run. */
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
            assert sContextWrapper.getBaseContext() == mContextWrapper;
            sContextWrapper.setBaseContext(ApplicationProvider.getApplicationContext());
            mClosed = true;

            if (mHttpFlagsInterceptor != null) mHttpFlagsInterceptor.close();

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
                mCronetEngine.stopNetLog();
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

    // Warning should go away once we can use java.util.function.Function.
    @SuppressWarnings("ImmutableEnumChecker")
    public enum CronetImplementation {
        STATICALLY_LINKED(
                context ->
                        (ExperimentalCronetEngine.Builder)
                                new NativeCronetProvider(context).createBuilder()),
        FALLBACK(
                (context) ->
                        (ExperimentalCronetEngine.Builder)
                                new JavaCronetProvider(context).createBuilder()),
        AOSP_PLATFORM(
                context ->
                        (ExperimentalCronetEngine.Builder)
                                new HttpEngineNativeProvider(context).createBuilder());

        private final EngineBuilderSupplier mEngineSupplier;

        private CronetImplementation(EngineBuilderSupplier engineSupplier) {
            this.mEngineSupplier = engineSupplier;
        }

        ExperimentalCronetEngine.Builder createBuilder(Context context) {
            return mEngineSupplier.getCronetEngineBuilder(context);
        }

        private void verifyCronetEngineInstance(CronetEngine engine) {
            switch (this) {
                case STATICALLY_LINKED:
                    assertThat(engine).isInstanceOf(CronetUrlRequestContext.class);
                    break;
                case FALLBACK:
                    assertThat(engine).isInstanceOf(JavaCronetEngine.class);
                    break;
                case AOSP_PLATFORM:
                    // We cannot reference the impl class for AOSP_PLATFORM. Do a reverse check
                    // instead.
                    assertThat(engine).isNotInstanceOf(CronetUrlRequestContext.class);
                    assertThat(engine).isNotInstanceOf(JavaCronetEngine.class);
                    break;
            }
        }

        private void checkImplClass(CronetEngine engine, Class expectedClass) {
            assertThat(engine).isInstanceOf(expectedClass);
        }
    }

    @Nullable
    private static <T extends Annotation> T getTestMethodAnnotation(
            Description description, Class<T> clazz) {
        return description.getAnnotation(clazz);
    }

    @Nullable
    private static <T extends Annotation> T getTestClassAnnotation(
            Description description, Class<T> clazz) {
        return description.getTestClass().getAnnotation(clazz);
    }

    private static String safeGetIgnoreReason(IgnoreFor ignoreAnnotation) {
        if (ignoreAnnotation == null) {
            return "";
        }
        return ignoreAnnotation.reason();
    }
}
