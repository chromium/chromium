// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net;

import android.content.Context;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.base.test.util.UrlUtils;
import org.chromium.net.test.ServerCertificate;

import java.util.Map;

/**
 * Wrapper class to start an in-process native test server, and get URLs needed to talk to it.
 *
 * <p>NativeTestServer only supports HTTP/1.
 */
@JNINamespace("cronet")
public final class NativeTestServer {
    // This variable contains the response body of a request to getSuccessURL().
    public static final String SUCCESS_BODY = "this is a text file\n";

    public static boolean startNativeTestServer(Context context) {
        if (!prepareNativeTestServer(context)) return false;
        startPrepared();
        return true;
    }

    public static boolean startNativeTestServerWithHTTPS(
            Context context, @ServerCertificate int serverCertificate) {
        if (!prepareNativeTestServerWithHTTPS(context, serverCertificate)) return false;
        startPrepared();
        return true;
    }

    public static boolean prepareNativeTestServer(Context context) {
        TestFilesInstaller.installIfNeeded(context);
        return NativeTestServerJni.get()
                .prepareNativeTestServer(
                        TestFilesInstaller.getInstalledPath(context),
                        UrlUtils.getIsolatedTestRoot(),
                        false, // useHttps
                        ServerCertificate.CERT_OK);
    }

    public static boolean prepareNativeTestServerWithHTTPS(
            Context context, @ServerCertificate int serverCertificate) {
        TestFilesInstaller.installIfNeeded(context);
        return NativeTestServerJni.get()
                .prepareNativeTestServer(
                        TestFilesInstaller.getInstalledPath(context),
                        UrlUtils.getIsolatedTestRoot(),
                        true, // useHttps
                        serverCertificate);
    }

    public static void startPrepared() {
        NativeTestServerJni.get().startPrepared();
    }

    public static void shutdownNativeTestServer() {
        NativeTestServerJni.get().shutdownNativeTestServer();
    }

    public static final class PreparedScope implements AutoCloseable {
        public PreparedScope(Context context) {
            if (!NativeTestServer.prepareNativeTestServer(context)) {
                throw new IllegalStateException("NativeTestServer already prepared");
            }
        }

        @Override
        public void close() {
            NativeTestServer.shutdownNativeTestServer();
        }
    }

    public static String getEchoBodyURL() {
        return NativeTestServerJni.get().getEchoBodyURL();
    }

    public static String getEchoHeaderURL(String header) {
        return NativeTestServerJni.get().getEchoHeaderURL(header);
    }

    public static String getEchoAllHeadersURL() {
        return NativeTestServerJni.get().getEchoAllHeadersURL();
    }

    public static String getEchoMethodURL() {
        return NativeTestServerJni.get().getEchoMethodURL();
    }

    public static String getRedirectToEchoBody() {
        return NativeTestServerJni.get().getRedirectToEchoBody();
    }

    public static String getFileURL(String filePath) {
        return NativeTestServerJni.get().getFileURL(filePath);
    }

    // Returns a URL that the server will return an Exabyte of data
    public static String getExabyteResponseURL() {
        return NativeTestServerJni.get().getExabyteResponseURL();
    }

    // The following URLs will make NativeTestServer serve a response based on
    // the contents of the corresponding file and its mock-http-headers file.

    public static String getSuccessURL() {
        return NativeTestServerJni.get().getFileURL("/success.txt");
    }

    public static String getRedirectURL() {
        return NativeTestServerJni.get().getFileURL("/redirect.html");
    }

    public static String getMultiRedirectURL() {
        return NativeTestServerJni.get().getFileURL("/multiredirect.html");
    }

    public static String getNotFoundURL() {
        return NativeTestServerJni.get().getFileURL("/notfound.html");
    }

    public static String getServerErrorURL() {
        return NativeTestServerJni.get().getFileURL("/server_error.txt");
    }

    public static int getPort() {
        return NativeTestServerJni.get().getPort();
    }

    public static String getHostPort() {
        return NativeTestServerJni.get().getHostPort();
    }

    /** Java counterpart of native net::test_server::HttpRequest. */
    public static final class HttpRequest {
        private final String mRelativeUrl;
        private final Map<String, String> mHeaders;
        private final String mMethod;
        private final String mAllHeaders;
        private final String mContent;

        public HttpRequest(
                String relativeUrl,
                Map<String, String> headers,
                String method,
                String allHeaders,
                String content) {
            mRelativeUrl = relativeUrl;
            mHeaders = headers;
            mMethod = method;
            mAllHeaders = allHeaders;
            mContent = content;
        }

        public String getRelativeUrl() {
            return mRelativeUrl;
        }

        public Map<String, String> getHeaders() {
            return mHeaders;
        }

        public String getMethod() {
            return mMethod;
        }

        public String getAllHeaders() {
            return mAllHeaders;
        }

        public String getContent() {
            return mContent;
        }
    }

    /** Java counterpart of native net::test_server::RawHttpResponse. */
    public static final class RawHttpResponse {
        private final String mHeaders;
        private final String mContents;

        public RawHttpResponse(String headers, String contents) {
            mHeaders = headers;
            mContents = contents;
        }

        public String getHeaders() {
            return mHeaders;
        }

        public String getContents() {
            return mContents;
        }
    }

    /** Java counterpart of native net::test_server::EmbeddedTestServer::HandleRequestCallback. */
    public static interface HandleRequestCallback {
        // Note currently we only support RawHttpResponse. We could add support for more flexible
        // response generation if need be.
        public RawHttpResponse handleRequest(HttpRequest httpRequest);
    }

    /** See net::test_server::EmbeddedTestServer::registerRequestHandler(). */
    public static void registerRequestHandler(HandleRequestCallback callback) {
        NativeTestServerJni.get().registerRequestHandler(callback);
    }

    // The following indirecting methods are needed because jni_zero doesn't support @CalledByNative
    // on nested classes. See https://crbug.com/422988765.

    @CalledByNative
    private static @JniType("cronet::NativeTestServerRawHttpResponse") RawHttpResponse
            handleRequest(
                    HandleRequestCallback callback,
                    @JniType("cronet::NativeTestServerHttpRequest") HttpRequest httpRequest) {
        return callback.handleRequest(httpRequest);
    }

    @CalledByNative
    private static HttpRequest createHttpRequest(
            @JniType("std::string") String relativeUrl,
            // The type alias is to work around https://crbug.com/422972348.
            @JniType("cronet::NativeTestServerHeaderMap") Map<String, String> headers,
            @JniType("std::string") String method,
            @JniType("std::string") String allHeaders,
            @JniType("std::string") String content) {
        return new HttpRequest(relativeUrl, headers, method, allHeaders, content);
    }

    @CalledByNative
    private static @JniType("std::string") String getRawHttpResponseHeaders(
            RawHttpResponse rawHttpResponse) {
        return rawHttpResponse.getHeaders();
    }

    @CalledByNative
    private static @JniType("std::string") String getRawHttpResponseContents(
            RawHttpResponse rawHttpResponse) {
        return rawHttpResponse.getContents();
    }

    @NativeMethods("cronet_tests")
    interface Natives {
        boolean prepareNativeTestServer(
                String filePath,
                String testDataDir,
                boolean useHttps,
                @ServerCertificate int certificate);

        void startPrepared();

        void shutdownNativeTestServer();

        String getEchoBodyURL();

        String getEchoHeaderURL(String header);

        String getEchoAllHeadersURL();

        String getEchoMethodURL();

        String getRedirectToEchoBody();

        String getFileURL(String filePath);

        String getExabyteResponseURL();

        String getHostPort();

        int getPort();

        void registerRequestHandler(
                @JniType("std::unique_ptr<cronet::NativeTestServerHandleRequestCallback>")
                        HandleRequestCallback callback);
    }
}
