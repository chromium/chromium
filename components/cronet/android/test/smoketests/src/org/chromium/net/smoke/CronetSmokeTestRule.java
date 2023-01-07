// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net.smoke;

import android.content.Context;
import android.support.test.InstrumentationRegistry;

import org.junit.Assert;
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
                new ExperimentalCronetEngine.Builder(InstrumentationRegistry.getTargetContext());
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
        Assert.assertEquals(SmokeTestRequestCallback.State.Succeeded, callback.getFinalState());

        // Check the response info
        UrlResponseInfo responseInfo = callback.getResponseInfo();
        Assert.assertNotNull(responseInfo);
        Assert.assertFalse(responseInfo.wasCached());
        Assert.assertEquals(url, responseInfo.getUrl());
        Assert.assertEquals(
                url, responseInfo.getUrlChain().get(responseInfo.getUrlChain().size() - 1));
        Assert.assertEquals(200, responseInfo.getHttpStatusCode());
        Assert.assertTrue(responseInfo.toString().length() > 0);
    }

    static void assertJavaEngine(CronetEngine engine) {
        Assert.assertNotNull(engine);
        Assert.assertEquals("org.chromium.net.impl.JavaCronetEngine", engine.getClass().getName());
    }

    static void assertNativeEngine(CronetEngine engine) {
        Assert.assertNotNull(engine);
        Assert.assertEquals(
                "org.chromium.net.impl.CronetUrlRequestContext", engine.getClass().getName());
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
        Context ctx = InstrumentationRegistry.getTargetContext();
        String packageName = ctx.getPackageName();
        int resId = ctx.getResources().getIdentifier(SUPPORT_IMPL_RES_KEY, "string", packageName);
        String className = ctx.getResources().getString(resId);
        Class<? extends TestSupport> cl = Class.forName(className).asSubclass(TestSupport.class);
        mTestSupport = cl.newInstance();
    }
}
