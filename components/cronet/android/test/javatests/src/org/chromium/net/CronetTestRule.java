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
import android.os.Build;
import android.os.ext.SdkExtensions;

import androidx.annotation.Nullable;
import androidx.test.core.app.ApplicationProvider;

import com.google.protobuf.ByteString;

import org.junit.rules.TestRule;
import org.junit.runner.Description;
import org.junit.runners.model.Statement;

import org.chromium.base.Log;
import org.chromium.base.test.util.DoNotBatch;
import org.chromium.build.BuildConfig;
import org.chromium.net.httpflags.FlagValue;
import org.chromium.net.impl.HttpEngineNativeProvider;
import org.chromium.net.impl.VersionSafeCallbacks;

import java.lang.annotation.Annotation;
import java.lang.annotation.ElementType;
import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.lang.annotation.Target;
import java.util.Arrays;
import java.util.EnumSet;

/** Custom TestRule for Cronet instrumentation tests. */
public class CronetTestRule implements TestRule {
    private static final String TAG = "CronetTestRule";

    private CronetTestFramework mCronetTestFramework;
    private CronetTestFramework.CronetImplementation mImplementation;

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
                // Internal error codes aren't supported in the fallback implementation and
                // inaccessible from AOSP_PLATFORM.
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
        return mImplementation.equals(CronetTestFramework.CronetImplementation.FALLBACK);
    }

    public CronetTestFramework.CronetImplementation implementationUnderTest() {
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
        setImplementationUnderTest(CronetTestFramework.CronetImplementation.STATICALLY_LINKED);
        String packageName = desc.getTestClass().getPackage().getName();
        String testName = desc.getTestClass().getName() + "#" + desc.getMethodName();

        // Find the API version required by the test.
        int requiredApiVersion = VersionSafeCallbacks.ApiVersion.getMaximumAvailableApiLevel();
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
                        + VersionSafeCallbacks.ApiVersion.getMaximumAvailableApiLevel()
                        + " is present.",
                VersionSafeCallbacks.ApiVersion.getMaximumAvailableApiLevel()
                        >= requiredApiVersion);
        assumeTrue(
                desc.getMethodName()
                        + " skipped because it Android's API level "
                        + requiredAndroidApiVersion
                        + " but test device supports only API "
                        + Build.VERSION.SDK_INT,
                Build.VERSION.SDK_INT >= requiredAndroidApiVersion);

        EnumSet<CronetTestFramework.CronetImplementation> implementationsUnderTest =
                EnumSet.allOf(CronetTestFramework.CronetImplementation.class);
        for (IgnoreFor ignoreFor :
                Arrays.asList(
                        getTestClassAnnotation(desc, IgnoreFor.class),
                        getTestMethodAnnotation(desc, IgnoreFor.class))) {
            if (ignoreFor == null) {
                continue;
            }
            implementationsUnderTest.removeAll(Arrays.asList(ignoreFor.implementations()));
            // SdkExtensions.getExtensionVersion is available starting from API level 30 (R).
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R
                    // HttpEngine ships to S+.
                    && SdkExtensions.getExtensionVersion(Build.VERSION_CODES.S)
                            >= ignoreFor.requiredSdkExtensionForPlatform()) {
                implementationsUnderTest.add(
                        CronetTestFramework.CronetImplementation.AOSP_PLATFORM);
            }
            // If not set to the requiredSdkExtensionForPlatform's sentiel value, always run within
            // the Android repository. See IgnoreFor#requiredSdkExtensionForPlatform's
            // documentation.
            if (BuildConfig.CRONET_FOR_AOSP_BUILD
                    && ignoreFor.requiredSdkExtensionForPlatform() != Integer.MAX_VALUE) {
                implementationsUnderTest.add(
                        CronetTestFramework.CronetImplementation.AOSP_PLATFORM);
            }
        }

        assertWithMessage(
                        "Test should not be skipped via IgnoreFor annotation. "
                                + "Use DisabledTest instead")
                .that(implementationsUnderTest)
                .isNotEmpty();

        if (!new HttpEngineNativeProvider(ApplicationProvider.getApplicationContext())
                .isEnabled()) {
            implementationsUnderTest.remove(CronetTestFramework.CronetImplementation.AOSP_PLATFORM);
            assumeFalse(
                    desc.getMethodName()
                            + " skipped because it's supposed to run against only AOSP_PLAFORM but"
                            + " test device is not U+",
                    implementationsUnderTest.isEmpty());
        }

        Log.i(TAG, "Implementations to be tested against: %s", implementationsUnderTest);

        if (packageName.startsWith("org.chromium.net")) {
            for (CronetTestFramework.CronetImplementation implementation :
                    implementationsUnderTest) {
                if (BuildConfig.CRONET_FOR_AOSP_BUILD
                        && implementation.equals(
                                CronetTestFramework.CronetImplementation.FALLBACK)) {
                    // Skip executing tests for JavaCronetEngine.
                    continue;
                }
                Log.i(TAG, "Running test against " + implementation + " implementation.");
                setImplementationUnderTest(implementation);
                evaluateWithFramework(base, testName, netLogEnabled, desc);
            }
        } else {
            evaluateWithFramework(base, testName, netLogEnabled, desc);
        }
    }

    // Backward compatibility layer for when CronetTestFramework was part of CronetTestRule. Ideally
    // we should update calling code to call directly into CronetTestFramework#getTestStorage, but
    // that is very low-priority.
    public static String getTestStorage(Context context) {
        return CronetTestFramework.getTestStorage(context);
    }

    private org.chromium.net.httpflags.Flags getDeclaredHttpFlags(Description desc) {
        Flags httpFlags = getTestMethodAnnotation(desc, Flags.class);
        if (httpFlags == null) {
            return null;
        }
        if (getTestClassAnnotation(desc, DoNotBatch.class) == null) {
            throw new IllegalStateException(
                    "Using @Flags annotation requires the test methods to be run individually by"
                            + " applying @DoNotBatch annotation to the test suite.");
        }
        org.chromium.net.httpflags.Flags.Builder flagsBuilder =
                org.chromium.net.httpflags.Flags.newBuilder();
        for (IntFlag flag : httpFlags.intFlags()) {
            flagsBuilder.putFlags(
                    flag.name(),
                    FlagValue.newBuilder()
                            .addConstrainedValues(
                                    FlagValue.ConstrainedValue.newBuilder()
                                            .setIntValue(flag.value()))
                            .build());
        }
        for (BoolFlag flag : httpFlags.boolFlags()) {
            flagsBuilder.putFlags(
                    flag.name(),
                    FlagValue.newBuilder()
                            .addConstrainedValues(
                                    FlagValue.ConstrainedValue.newBuilder()
                                            .setBoolValue(flag.value()))
                            .build());
        }
        for (FloatFlag flag : httpFlags.floatFlags()) {
            flagsBuilder.putFlags(
                    flag.name(),
                    FlagValue.newBuilder()
                            .addConstrainedValues(
                                    FlagValue.ConstrainedValue.newBuilder()
                                            .setFloatValue(flag.value()))
                            .build());
        }
        for (StringFlag flag : httpFlags.stringFlags()) {
            flagsBuilder.putFlags(
                    flag.name(),
                    FlagValue.newBuilder()
                            .addConstrainedValues(
                                    FlagValue.ConstrainedValue.newBuilder()
                                            .setStringValue(flag.value()))
                            .build());
        }
        for (BytesFlag flag : httpFlags.bytesFlags()) {
            flagsBuilder.putFlags(
                    flag.name(),
                    FlagValue.newBuilder()
                            .addConstrainedValues(
                                    FlagValue.ConstrainedValue.newBuilder()
                                            .setBytesValue(ByteString.copyFrom(flag.value())))
                            .build());
        }
        return flagsBuilder.build();
    }

    private void evaluateWithFramework(
            Statement statement, String testName, boolean netLogEnabled, Description desc)
            throws Throwable {
        org.chromium.net.httpflags.Flags flags = getDeclaredHttpFlags(desc);
        try (CronetTestFramework framework =
                createCronetTestFramework(testName, netLogEnabled, flags)) {
            statement.evaluate();
        } finally {
            mCronetTestFramework = null;
        }
    }

    private CronetTestFramework createCronetTestFramework(
            String testName, boolean netLogEnabled, org.chromium.net.httpflags.Flags flags) {
        mCronetTestFramework =
                new CronetTestFramework(mImplementation, testName, netLogEnabled, flags);
        if (mEngineStartupMode.equals(EngineStartupMode.AUTOMATIC)) {
            mCronetTestFramework.startEngine();
        }
        return mCronetTestFramework;
    }

    /**
     * Annotation allowing classes or individual tests to be skipped based on the implementation
     * being currently tested. When this annotation is present the test is only run against the
     * {@link CronetTestFramework.CronetImplementation} cases not specified in the annotation. If
     * the annotation is specified both at the class and method levels, the union of
     * IgnoreFor#implementations() will be skipped.
     */
    @Target({ElementType.TYPE, ElementType.METHOD})
    @Retention(RetentionPolicy.RUNTIME)
    public @interface IgnoreFor {
        CronetTestFramework.CronetImplementation[] implementations();

        String reason();

        /**
         * Controls more strictly whether a test should, or should not, run against AOSP_PLATFORM.
         *
         * <p>Integer.MAX_VALUE is a special sentiel value. If set to that, the test will will never
         * run against AOSP_PLATFORM.
         *
         * <p>For any other value, the test will only run against AOSP_PLATFORM when it bundles the
         * necessary code for the test to pass. This differs depending on whether the test is
         * running within, or outside, the Android repository.
         *
         * <p>More precisely; if the test is running within the Android repository, the test code
         * and code being tested are always in sync. With that in mind, AOSP_PLATFORM always bundles
         * the code necesary for the test to run. If the test is running outside of the Android
         * repository, the test code and code being tested are never in sync: the code being tested
         * comes from the system image installed on the device, while the test code comes from the
         * local repository. In this scenario, we can only run the test if the device has a recent
         * enough SDK extension.
         *
         * <p>For tests that rely on Cronet internals, not accessible from AOSP_PLATFORM, this
         * should be set to Integer.MAX_VALUE. For tests that don't, this should be set to the SDK
         * extension that shipped a version of HttpEngine that contains the necessary Cronet changes
         * (see http://go/android-sdk-docs-sdk-extensions).
         */
        int requiredSdkExtensionForPlatform() default Integer.MAX_VALUE;
    }

    /**
     * Annotation allowing classes or individual tests to run with a specific httpflag turned on.
     * This does not mean that the test will run multiple times, it means that the test will run
     * only once with the specified flags enabled.
     *
     * <p>Warning: The test must not use @Batch as the httpflags loading happens only once when
     * Cronet itself is initialized on startup. This may lead to inconsistent global state depending
     * on the order of the test-execution (eg: flags may be applied to undesired tests). The test
     * rule protects against that by asserting that the test suite is never batched.
     */
    @Target({ElementType.TYPE, ElementType.METHOD})
    @Retention(RetentionPolicy.RUNTIME)
    public @interface Flags {
        IntFlag[] intFlags() default {};

        StringFlag[] stringFlags() default {};

        BoolFlag[] boolFlags() default {};

        FloatFlag[] floatFlags() default {};

        BytesFlag[] bytesFlags() default {};
    }

    public @interface IntFlag {
        String name();

        long value();
    }

    public @interface StringFlag {
        String name();

        String value();
    }

    public @interface BoolFlag {
        String name();

        boolean value();
    }

    public @interface FloatFlag {
        String name();

        float value();
    }

    public @interface BytesFlag {
        String name();

        byte[] value();
    }

    /**
     * Annotation allowing classes or individual tests to be skipped based on the version of the
     * Cronet API present. Takes the minimum API version upon which the test should be run. For
     * example if a test should only be run with API version 2 or greater: @RequiresMinApi(2) public
     * void testFoo() {}
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

    private void setImplementationUnderTest(
            CronetTestFramework.CronetImplementation implementation) {
        mImplementation = implementation;
    }

    private enum EngineStartupMode {
        MANUAL,
        AUTOMATIC,
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
}
