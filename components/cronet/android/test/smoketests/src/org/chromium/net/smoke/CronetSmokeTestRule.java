// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net.smoke;

import static com.google.common.truth.Truth.assertThat;

import static org.chromium.net.truth.UrlResponseInfoSubject.assertThat;

import android.content.Context;

import androidx.test.core.app.ApplicationProvider;

import org.junit.rules.TestRule;
import org.junit.runner.Description;
import org.junit.runners.model.Statement;

import org.chromium.net.CronetEngine;
import org.chromium.net.ExperimentalCronetEngine;
import org.chromium.net.UrlResponseInfo;

/**
 * Base test class. This class should not import any classes from the org.chromium.base package.
 */
public class CronetSmokeTestRule implements TestRule {
    /**
     * The key in the string resource file that specifies {@link TestSupport} that should
     * be instantiated.
     */
    private static final String SUPPORT_IMPL_RES_KEY = "TestSupportImplClass";

    public ExperimentalCronetEngine.Builder mCronetEngineBuilder;
    public CronetEngine mCronetEngine;
    public TestSupport mTestSupport;

    @Override
    public Statement apply(final Statement base, Description desc) {
        return new Statement() {
            @Override
            public void evaluate() throws Throwable {
                ruleSetUp();
                base.evaluate();
                ruleTearDown();
            }
        };
    }

    public TestSupport getTestSupport() {
        return mTestSupport;
    }

    public CronetEngine getCronetEngine() {
        return mCronetEngine;
    }

    public ExperimentalCronetEngine.Builder getCronetEngineBuilder() {
        return mCronetEngineBuilder;
    }

    private void ruleSetUp() throws Exception {
        mCronetEngineBuilder =
                new ExperimentalCronetEngine.Builder(ApplicationProvider.getApplicationContext());
        initTestSupport();
    }

    private void ruleTearDown() throws Exception {
        if (mCronetEngine != null) {
            mCronetEngine.shutdown();
        }
    }

    public void initCronetEngine() {
        mCronetEngine = mCronetEngineBuilder.build();
    }

    static void assertSuccessfulNonEmptyResponse(SmokeTestRequestCallback callback, String url) {
        // Check the request state
        if (callback.getFinalState() == SmokeTestRequestCallback.State.Failed) {
            throw new RuntimeException(
                    "The request failed with an error", callback.getFailureError());
        }
        assertThat(callback.getFinalState()).isEqualTo(SmokeTestRequestCallback.State.Succeeded);

        // Check the response info
        UrlResponseInfo responseInfo = callback.getResponseInfo();
        assertThat(responseInfo).isNotNull();
        assertThat(responseInfo).wasNotCached();
        assertThat(responseInfo).hasUrlThat().isEqualTo(url);
        assertThat(responseInfo.getUrlChain().get(responseInfo.getUrlChain().size() - 1))
                .isEqualTo(url);
        assertThat(responseInfo).hasHttpStatusCodeThat().isEqualTo(200);
        assertThat(responseInfo.toString()).isNotEmpty();
    }

    static void assertJavaEngine(CronetEngine engine) {
        assertThat(engine).isNotNull();
        assertThat(engine.getClass().getName()).isEqualTo("org.chromium.net.impl.JavaCronetEngine");
    }

    static void assertNativeEngine(CronetEngine engine) {
        assertThat(engine).isNotNull();
        assertThat(engine.getClass().getName())
                .isEqualTo("org.chromium.net.impl.CronetUrlRequestContext");
    }

    /**
     * Instantiates a concrete implementation of {@link TestSupport} interface.
     * The name of the implementation class is determined dynamically by reading
     * the value of |TestSupportImplClass| from the Android string resource file.
     *
     * @throws Exception if the class cannot be instantiated.
     */
    @SuppressWarnings("DiscouragedApi")
    private void initTestSupport() throws Exception {
        Context ctx = ApplicationProvider.getApplicationContext();
        String packageName = ctx.getPackageName();
        int resId = ctx.getResources().getIdentifier(SUPPORT_IMPL_RES_KEY, "string", packageName);
        String className = ctx.getResources().getString(resId);
        Class<? extends TestSupport> cl = Class.forName(className).asSubclass(TestSupport.class);
        mTestSupport = cl.newInstance();
    }
}
