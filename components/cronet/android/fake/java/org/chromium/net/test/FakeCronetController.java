// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net.test;

import android.content.Context;

import org.chromium.net.CronetEngine;
import org.chromium.net.ExperimentalCronetEngine;
import org.chromium.net.UrlRequest;

import java.util.ArrayList;
import java.util.Collections;
import java.util.List;
import java.util.Map;

/**
 * Controller for fake Cronet implementation. Allows a test to setup responses for
 * {@link UrlRequest}s. If multiple {@link ResponseMatcher}s match a specific request, the first
 * {@link ResponseMatcher} added takes precedence.
 */
public final class FakeCronetController {
    // List of FakeCronetEngines so that FakeCronetEngine can be accessed when created with
    // the {@link FakeCronetProvider}.
    private static final List<CronetEngine> sInstances =
            Collections.synchronizedList(new ArrayList<>());

    // List of ResponseMatchers to be checked for a response to a request in place of a server.
    private final List<ResponseMatcher> mResponseMatchers =
            Collections.synchronizedList(new ArrayList<>());

    /**
     * Creates a fake {@link CronetEngine.Builder} that creates {@link CronetEngine}s that return
     * fake {@link UrlRequests}. Once built, the {@link CronetEngine}'s {@link UrlRequest}s will
     * retrieve responses from this {@link FakeCronetController}.
     *
     * @param context the Android context to build the fake {@link CronetEngine} from.
     * @return a fake CronetEngine.Builder that uses this {@link FakeCronetController} to manage
     * responses once it is built.
     */
    public CronetEngine.Builder newFakeCronetEngineBuilder(Context context) {
        FakeCronetEngine.Builder builder = new FakeCronetEngine.Builder(context);
        builder.setController(this);
        // FakeCronetEngine.Builder is not actually a CronetEngine.Builder, so construct one with
        // the child of CronetEngine.Builder: ExperimentalCronetEngine.Builder.
        return new ExperimentalCronetEngine.Builder(builder);
    }

    /**
     * Adds a {@link UrlResponseMatcher} that will respond to the provided URL with the provided
     * {@link FakeUrlResponse}. Equivalent to:
     * addResponseMatcher(new UrlResponseMatcher(url, response)).
     *
     * @param response a {@link FakeUrlResponse} to respond with
     * @param url      a url for which the response should be returned
     */
    public void addResponseForUrl(FakeUrlResponse response, String url) {
        addResponseMatcher(new UrlResponseMatcher(url, response));
    }

    /**
     * Adds a {@link ResponseMatcher} to the list of {@link ResponseMatcher}s.
     *
     * @param matcher the {@link ResponseMatcher} that should be matched against a request
     */
    public void addResponseMatcher(ResponseMatcher matcher) {
        mResponseMatchers.add(matcher);
    }

    /**
     * Removes a specific {@link ResponseMatcher} from the list of {@link ResponseMatcher}s.
     *
     * @param matcher the {@link ResponseMatcher} to remove
     */
    public void removeResponseMatcher(ResponseMatcher matcher) {
        mResponseMatchers.remove(matcher);
    }

    /**
     * Removes all {@link ResponseMatcher}s from the list of {@link ResponseMatcher}s.
     */
    public void clearResponseMatchers() {
        mResponseMatchers.clear();
    }

    /**
     * Adds a {@link FakeUrlResponse} to the list of responses that will redirect a
     * {@link UrlRequest} to the specified URL.
     *
     * @param redirectLocation the URL to redirect the {@link UrlRequest} to
     * @param url              the URL that will trigger the redirect
     */
    public void addRedirectResponse(String redirectLocation, String url) {
        FakeUrlResponse redirectResponse = new FakeUrlResponse.Builder()
                                                   .setHttpStatusCode(302)
                                                   .addHeader("location", redirectLocation)
                                                   .build();
        addResponseForUrl(redirectResponse, url);
    }

    /**
     * Adds an {@link FakeUrlResponse} that fails with the specified HTTP code for the specified
     * URL.
     *
     * @param statusCode the code for the {@link FakeUrlResponse}
     * @param url        the URL that should trigger the error response when requested by a
     *                   {@link UrlRequest}
     * @throws IllegalArgumentException if the HTTP status code is not an error code (code >= 400)
     */
    public void addHttpErrorResponse(int statusCode, String url) {
        addResponseForUrl(getFailedResponse(statusCode), url);
    }

    // TODO(kirchman): Create a function to add a response that takes a CronetException.

    /**
     * Adds a successful 200 code {@link FakeUrlResponse} that will match the specified
     * URL when requested by a {@link UrlRequest}.
     *
     * @param url the URL that triggers the successful response
     * @param body the body of the response as a byte array
     */
    public void addSuccessResponse(String url, byte[] body) {
        addResponseForUrl(new FakeUrlResponse.Builder().setResponseBody(body).build(), url);
    }

    /**
     * Returns the {@link CronetEngineController} for a specified {@link CronetEngine}. This method
     * should be used in conjunction with {@link FakeCronetController.getInstances}.
     *
     * @param engine the fake {@link CronetEngine} to get the controller for.
     * @return the controller for the specified fake {@link CronetEngine}.
     */
    public static FakeCronetController getControllerForFakeEngine(CronetEngine engine) {
        if (engine instanceof FakeCronetEngine) {
            FakeCronetEngine fakeEngine = (FakeCronetEngine) engine;
            return fakeEngine.getController();
        }
        throw new IllegalArgumentException("Provided CronetEngine is not a fake CronetEngine");
    }

    /**
     * Returns all created fake instances of {@link CronetEngine} that have not been shut down with
     * {@link CronetEngine.shutdown()} in order of creation. Can be used to retrieve a controller
     * in conjunction with {@link FakeCronetController.getControllerForFakeEngine}.
     *
     * @return a list of all fake {@link CronetEngine}s that have been created
     */
    public static List<CronetEngine> getFakeCronetEngines() {
        synchronized (sInstances) {
            return new ArrayList<>(sInstances);
        }
    }

    /**
     * Removes a fake {@link CronetEngine} from the list of {@link CronetEngine} instances.
     *
     * @param cronetEngine the instance to remove
     */
    static void removeFakeCronetEngine(CronetEngine cronetEngine) {
        sInstances.remove(cronetEngine);
    }

    /**
     * Add a CronetEngine to the list of CronetEngines.
     *
     * @param engine the {@link CronetEngine} to add
     */
    static void addFakeCronetEngine(FakeCronetEngine engine) {
        sInstances.add(engine);
    }

    /**
     * Gets a response for specified request details if there is one, or a "404" failed response
     * if there is no {@link ResponseMatcher} with a {@link FakeUrlResponse} for this request.
     *
     * @param url        the URL that the {@link UrlRequest} is connecting to
     * @param httpMethod the HTTP method that the {@link UrlRequest} is using to connect with
     * @param headers    the headers supplied by the {@link UrlRequest}
     * @param body       the body of the fake HTTP request
     * @return a {@link FakeUrlResponse} if there is one, or a failed "404" response if none found
     */
    FakeUrlResponse getResponse(
            String url, String httpMethod, List<Map.Entry<String, String>> headers, byte[] body) {
        synchronized (mResponseMatchers) {
            for (ResponseMatcher responseMatcher : mResponseMatchers) {
                FakeUrlResponse matchedResponse =
                        responseMatcher.getMatchingResponse(url, httpMethod, headers, body);
                if (matchedResponse != null) {
                    return matchedResponse;
                }
            }
        }
        return getFailedResponse(404);
    }

    /**
     * Creates and returns a failed response with the specified HTTP status code.
     *
     * @param statusCode the HTTP code that the returned response will have
     * @return a {@link FakeUrlResponse} with the specified code
     */
    private static FakeUrlResponse getFailedResponse(int statusCode) {
        if (statusCode < 400) {
            throw new IllegalArgumentException(
                    "Expected HTTP error code (code >= 400), but was: " + statusCode);
        }
        return new FakeUrlResponse.Builder().setHttpStatusCode(statusCode).build();
    }
}
