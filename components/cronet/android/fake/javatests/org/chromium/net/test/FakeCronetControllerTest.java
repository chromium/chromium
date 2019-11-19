// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net.test;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;
import static org.junit.Assert.fail;

import android.content.Context;
import android.support.test.InstrumentationRegistry;
import android.support.test.filters.SmallTest;
import android.support.test.runner.AndroidJUnit4;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.net.CronetEngine;
import org.chromium.net.impl.JavaCronetEngineBuilderImpl;

import java.util.AbstractMap;
import java.util.List;
import java.util.Map;

/**
 * Test functionality of {@link FakeCronetController}.
 */
@RunWith(AndroidJUnit4.class)
public class FakeCronetControllerTest {
    Context mContext;
    FakeCronetController mFakeCronetController;

    @Before
    public void setUp() {
        mContext = InstrumentationRegistry.getTargetContext();
        mFakeCronetController = new FakeCronetController();
    }

    @Test
    @SmallTest
    public void testGetFakeCronetEnginesStartsEmpty() {
        List<CronetEngine> engines = FakeCronetController.getFakeCronetEngines();
        assertEquals(0, engines.size());
    }

    @Test
    @SmallTest
    public void testGetFakeCronetEnginesIncludesCreatedEngineInOrder() {
        // Create an instance with the controller.
        CronetEngine engine = mFakeCronetController.newFakeCronetEngineBuilder(mContext).build();
        // Create an instance with the provider.
        FakeCronetProvider provider = new FakeCronetProvider(mContext);
        CronetEngine providerEngine = provider.createBuilder().build();
        List<CronetEngine> engines = FakeCronetController.getFakeCronetEngines();

        assertTrue(engines.contains(engine));
        assertTrue(engines.contains(providerEngine));
        assertEquals(engine, engines.get(0));
        assertEquals(providerEngine, engines.get(1));
    }

    @Test
    @SmallTest
    public void testGetControllerGetsCorrectController() {
        // Create an instance with the controller.
        CronetEngine engine = mFakeCronetController.newFakeCronetEngineBuilder(mContext).build();
        CronetEngine engine2 = mFakeCronetController.newFakeCronetEngineBuilder(mContext).build();

        // Create two engines with a second controller.
        FakeCronetController newController = new FakeCronetController();
        CronetEngine newControllerEngine =
                newController.newFakeCronetEngineBuilder(mContext).build();
        CronetEngine newControllerEngine2 =
                newController.newFakeCronetEngineBuilder(mContext).build();

        // Create an instance with the provider.
        FakeCronetProvider provider = new FakeCronetProvider(mContext);
        CronetEngine providerEngine = provider.createBuilder().build();

        assertEquals(
                mFakeCronetController, FakeCronetController.getControllerForFakeEngine(engine));
        assertEquals(
                mFakeCronetController, FakeCronetController.getControllerForFakeEngine(engine2));
        assertEquals(newController,
                FakeCronetController.getControllerForFakeEngine(newControllerEngine));
        assertEquals(newController,
                FakeCronetController.getControllerForFakeEngine(newControllerEngine2));

        // TODO(kirchman): Test which controller the provider-created engine uses once the fake
        // UrlRequest class has been implemented.
        assertNotEquals(mFakeCronetController,
                FakeCronetController.getControllerForFakeEngine(providerEngine));
        assertNotEquals(
                newController, FakeCronetController.getControllerForFakeEngine(providerEngine));
        assertNotNull(FakeCronetController.getControllerForFakeEngine(providerEngine));
    }

    @Test
    @SmallTest
    public void testAddNonFakeCronetEngineNotAllowed() {
        CronetEngine javaEngine = new JavaCronetEngineBuilderImpl(mContext).build();

        try {
            FakeCronetController.getControllerForFakeEngine(javaEngine);
            fail("Should not be able to get a controller for a non-fake CronetEngine.");
        } catch (IllegalArgumentException e) {
            assertEquals("Provided CronetEngine is not a fake CronetEngine", e.getMessage());
        }
    }

    @Test
    @SmallTest
    public void testShutdownRemovesCronetEngine() {
        CronetEngine engine = mFakeCronetController.newFakeCronetEngineBuilder(mContext).build();
        CronetEngine engine2 = mFakeCronetController.newFakeCronetEngineBuilder(mContext).build();
        List<CronetEngine> engines = FakeCronetController.getFakeCronetEngines();
        assertTrue(engines.contains(engine));
        assertTrue(engines.contains(engine2));

        engine.shutdown();
        engines = FakeCronetController.getFakeCronetEngines();

        assertFalse(engines.contains(engine));
        assertTrue(engines.contains(engine2));
    }

    @Test
    @SmallTest
    public void testResponseMatchersConsultedInOrderOfAddition() {
        String url = "url";
        FakeUrlResponse response =
                new FakeUrlResponse.Builder().setResponseBody("body text".getBytes()).build();
        ResponseMatcher matcher = new UrlResponseMatcher(url, response);
        mFakeCronetController.addResponseMatcher(matcher);
        mFakeCronetController.addSuccessResponse(url, "different text".getBytes());

        FakeUrlResponse foundResponse =
                mFakeCronetController.getResponse(new String(url), null, null, null);

        assertEquals(response, foundResponse);
    }

    @Test
    @SmallTest
    public void testRemoveResponseMatcher() {
        String url = "url";
        FakeUrlResponse response =
                new FakeUrlResponse.Builder().setResponseBody("body text".getBytes()).build();
        ResponseMatcher matcher = new UrlResponseMatcher(url, response);
        mFakeCronetController.addResponseMatcher(matcher);
        mFakeCronetController.removeResponseMatcher(matcher);

        FakeUrlResponse foundResponse = mFakeCronetController.getResponse(url, null, null, null);

        assertEquals(404, foundResponse.getHttpStatusCode());
        assertNotEquals(response, foundResponse);
    }

    @Test
    @SmallTest
    public void testClearResponseMatchers() {
        String url = "url";
        FakeUrlResponse response =
                new FakeUrlResponse.Builder().setResponseBody("body text".getBytes()).build();
        ResponseMatcher matcher = new UrlResponseMatcher(url, response);
        mFakeCronetController.addResponseMatcher(matcher);
        mFakeCronetController.clearResponseMatchers();

        FakeUrlResponse foundResponse = mFakeCronetController.getResponse(url, null, null, null);

        assertEquals(404, foundResponse.getHttpStatusCode());
        assertNotEquals(response, foundResponse);
    }

    @Test
    @SmallTest
    public void testAddUrlResponseMatcher() {
        String url = "url";
        FakeUrlResponse response =
                new FakeUrlResponse.Builder().setResponseBody("body text".getBytes()).build();
        mFakeCronetController.addResponseForUrl(response, url);

        FakeUrlResponse foundResponse = mFakeCronetController.getResponse(url, null, null, null);

        assertEquals(foundResponse, response);
    }

    @Test
    @SmallTest
    public void testDefaultResponseIs404() {
        FakeUrlResponse foundResponse = mFakeCronetController.getResponse("url", null, null, null);

        assertEquals(404, foundResponse.getHttpStatusCode());
    }

    @Test
    @SmallTest
    public void testAddRedirectResponse() {
        String url = "url";
        String location = "/TEST_REDIRECT_LOCATION";
        mFakeCronetController.addRedirectResponse(location, url);

        FakeUrlResponse foundResponse = mFakeCronetController.getResponse("url", null, null, null);
        Map.Entry<String, String> headerEntry = new AbstractMap.SimpleEntry<>("location", location);

        assertTrue(foundResponse.getAllHeadersList().contains(headerEntry));
        assertTrue(foundResponse.getHttpStatusCode() >= 300);
        assertTrue(foundResponse.getHttpStatusCode() < 400);
    }

    @Test
    @SmallTest
    public void testAddErrorResponse() {
        String url = "url";
        int httpStatusCode = 400;
        mFakeCronetController.addHttpErrorResponse(httpStatusCode, url);

        FakeUrlResponse foundResponse = mFakeCronetController.getResponse(url, null, null, null);

        assertEquals(foundResponse.getHttpStatusCode(), httpStatusCode);
    }

    @Test
    @SmallTest
    public void testAddErrorResponseWithNonErrorCodeThrowsException() {
        int nonErrorCode = 200;
        try {
            mFakeCronetController.addHttpErrorResponse(nonErrorCode, "url");
            fail("Should not be able to add an error response with a non-error code.");
        } catch (IllegalArgumentException e) {
            assertEquals("Expected HTTP error code (code >= 400), but was: " + nonErrorCode,
                    e.getMessage());
        }
    }

    @Test
    @SmallTest
    public void testAddSuccessResponse() {
        String url = "url";
        String body = "TEST_BODY";
        mFakeCronetController.addSuccessResponse(url, body.getBytes());

        FakeUrlResponse foundResponse = mFakeCronetController.getResponse(url, null, null, null);

        assertTrue(foundResponse.getHttpStatusCode() >= 200);
        assertTrue(foundResponse.getHttpStatusCode() < 300);
        assertEquals(body, new String(foundResponse.getResponseBody()));
    }
}
