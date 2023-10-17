// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net.test;

import static com.google.common.truth.Truth.assertThat;

import static org.junit.Assert.assertThrows;

import android.content.Context;

import androidx.test.core.app.ApplicationProvider;
import androidx.test.ext.junit.runners.AndroidJUnit4;
import androidx.test.filters.SmallTest;

import com.google.common.collect.Range;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.net.CronetEngine;
import org.chromium.net.impl.JavaCronetEngineBuilderImpl;

import java.util.AbstractMap;
import java.util.List;
import java.util.Map;

/** Test functionality of {@link FakeCronetController}. */
@RunWith(AndroidJUnit4.class)
public class FakeCronetControllerTest {
    Context mContext;
    FakeCronetController mFakeCronetController;

    @Before
    public void setUp() {
        mContext = ApplicationProvider.getApplicationContext();
        mFakeCronetController = new FakeCronetController();
    }

    @Test
    @SmallTest
    public void testGetFakeCronetEnginesStartsEmpty() {
        List<CronetEngine> engines = FakeCronetController.getFakeCronetEngines();
        assertThat(engines).isEmpty();
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

        assertThat(engines).containsExactly(engine, providerEngine).inOrder();
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

        assertThat(FakeCronetController.getControllerForFakeEngine(engine))
                .isEqualTo(mFakeCronetController);
        assertThat(FakeCronetController.getControllerForFakeEngine(engine2))
                .isEqualTo(mFakeCronetController);
        assertThat(FakeCronetController.getControllerForFakeEngine(newControllerEngine))
                .isEqualTo(newController);
        assertThat(FakeCronetController.getControllerForFakeEngine(newControllerEngine2))
                .isEqualTo(newController);

        // TODO(kirchman): Test which controller the provider-created engine uses once the fake
        // UrlRequest class has been implemented.
        assertThat(FakeCronetController.getControllerForFakeEngine(providerEngine))
                .isNotEqualTo(mFakeCronetController);
        assertThat(FakeCronetController.getControllerForFakeEngine(providerEngine))
                .isNotEqualTo(newController);
        assertThat(FakeCronetController.getControllerForFakeEngine(providerEngine)).isNotNull();
    }

    @Test
    @SmallTest
    public void testAddNonFakeCronetEngineNotAllowed() {
        CronetEngine javaEngine = new JavaCronetEngineBuilderImpl(mContext).build();

        IllegalArgumentException e =
                assertThrows(
                        IllegalArgumentException.class,
                        () -> FakeCronetController.getControllerForFakeEngine(javaEngine));
        assertThat(e)
                .hasMessageThat()
                .isEqualTo("Provided CronetEngine is not a fake CronetEngine");
    }

    @Test
    @SmallTest
    public void testShutdownRemovesCronetEngine() {
        CronetEngine engine = mFakeCronetController.newFakeCronetEngineBuilder(mContext).build();
        CronetEngine engine2 = mFakeCronetController.newFakeCronetEngineBuilder(mContext).build();
        List<CronetEngine> engines = FakeCronetController.getFakeCronetEngines();
        assertThat(engines).containsExactly(engine, engine2);

        engine.shutdown();
        engines = FakeCronetController.getFakeCronetEngines();

        assertThat(engines).containsExactly(engine2);
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

        FakeUrlResponse foundResponse = mFakeCronetController.getResponse(url, null, null, null);

        assertThat(foundResponse).isEqualTo(response);
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

        assertThat(foundResponse.getHttpStatusCode()).isEqualTo(404);
        assertThat(foundResponse).isNotEqualTo(response);
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

        assertThat(foundResponse.getHttpStatusCode()).isEqualTo(404);
        assertThat(foundResponse).isNotEqualTo(response);
    }

    @Test
    @SmallTest
    public void testAddUrlResponseMatcher() {
        String url = "url";
        FakeUrlResponse response =
                new FakeUrlResponse.Builder().setResponseBody("body text".getBytes()).build();
        mFakeCronetController.addResponseForUrl(response, url);

        FakeUrlResponse foundResponse = mFakeCronetController.getResponse(url, null, null, null);

        assertThat(foundResponse).isEqualTo(response);
    }

    @Test
    @SmallTest
    public void testDefaultResponseIs404() {
        FakeUrlResponse foundResponse = mFakeCronetController.getResponse("url", null, null, null);

        assertThat(foundResponse.getHttpStatusCode()).isEqualTo(404);
    }

    @Test
    @SmallTest
    public void testAddRedirectResponse() {
        String url = "url";
        String location = "/TEST_REDIRECT_LOCATION";
        mFakeCronetController.addRedirectResponse(location, url);

        FakeUrlResponse foundResponse = mFakeCronetController.getResponse("url", null, null, null);
        Map.Entry<String, String> headerEntry = new AbstractMap.SimpleEntry<>("location", location);

        assertThat(foundResponse.getAllHeadersList()).contains(headerEntry);
        assertThat(foundResponse.getHttpStatusCode()).isIn(Range.closedOpen(300, 400));
    }

    @Test
    @SmallTest
    public void testAddErrorResponse() {
        String url = "url";
        int httpStatusCode = 400;
        mFakeCronetController.addHttpErrorResponse(httpStatusCode, url);

        FakeUrlResponse foundResponse = mFakeCronetController.getResponse(url, null, null, null);

        assertThat(foundResponse.getHttpStatusCode()).isEqualTo(httpStatusCode);
    }

    @Test
    @SmallTest
    public void testAddErrorResponseWithNonErrorCodeThrowsException() {
        int nonErrorCode = 200;
        IllegalArgumentException e =
                assertThrows(
                        IllegalArgumentException.class,
                        () -> mFakeCronetController.addHttpErrorResponse(nonErrorCode, "url"));
        assertThat(e)
                .hasMessageThat()
                .isEqualTo("Expected HTTP error code (code >= 400), but was: " + nonErrorCode);
    }

    @Test
    @SmallTest
    public void testAddSuccessResponse() {
        String url = "url";
        String body = "TEST_BODY";
        mFakeCronetController.addSuccessResponse(url, body.getBytes());

        FakeUrlResponse foundResponse = mFakeCronetController.getResponse(url, null, null, null);

        assertThat(foundResponse.getHttpStatusCode()).isIn(Range.closedOpen(200, 300));
        assertThat(new String(foundResponse.getResponseBody())).isEqualTo(body);
    }
}
