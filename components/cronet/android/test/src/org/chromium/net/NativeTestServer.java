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

import java.util.List;
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

    private static Long sEmbeddedTestServerAdapter;

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
        if (sEmbeddedTestServerAdapter != null) {
            return false;
        }
        TestFilesInstaller.installIfNeeded(context);
        sEmbeddedTestServerAdapter =
                NativeTestServerJni.get()
                        .create(
                                TestFilesInstaller.getInstalledPath(context),
                                UrlUtils.getIsolatedTestRoot(),
                                false, // useHttps
                                ServerCertificate.CERT_OK);
        return true;
    }

    public static boolean prepareNativeTestServerWithHTTPS(
            Context context, @ServerCertificate int serverCertificate) {
        if (sEmbeddedTestServerAdapter != null) {
            return false;
        }
        TestFilesInstaller.installIfNeeded(context);
        sEmbeddedTestServerAdapter =
                NativeTestServerJni.get()
                        .create(
                                TestFilesInstaller.getInstalledPath(context),
                                UrlUtils.getIsolatedTestRoot(),
                                true, // useHttps
                                serverCertificate);
        return true;
    }

    public static void enableConnectProxy(List<String> urlsToBeProxied) {
        NativeTestServerJni.get()
                .enableConnectProxy(
                        sEmbeddedTestServerAdapter, urlsToBeProxied.toArray(new String[0]));
    }

    public static void startPrepared() {
        NativeTestServerJni.get().start(sEmbeddedTestServerAdapter);
    }

    public static void shutdownNativeTestServer() {
        if (sEmbeddedTestServerAdapter == null) {
            return;
        }
        NativeTestServerJni.get().destroy(sEmbeddedTestServerAdapter);
        sEmbeddedTestServerAdapter = null;
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
        return NativeTestServerJni.get().getEchoBodyURL(sEmbeddedTestServerAdapter);
    }

    public static String getEchoHeaderURL(String header) {
        return NativeTestServerJni.get().getEchoHeaderURL(sEmbeddedTestServerAdapter, header);
    }

    public static String getEchoAllHeadersURL() {
        return NativeTestServerJni.get().getEchoAllHeadersURL(sEmbeddedTestServerAdapter);
    }

    public static String getEchoMethodURL() {
        return NativeTestServerJni.get().getEchoMethodURL(sEmbeddedTestServerAdapter);
    }

    public static String getUseEncodingURL(String encoding) {
        return NativeTestServerJni.get().getUseEncodingURL(sEmbeddedTestServerAdapter, encoding);
    }

    public static String getRedirectToEchoBody() {
        return NativeTestServerJni.get().getRedirectToEchoBodyURL(sEmbeddedTestServerAdapter);
    }

    public static String getFileURL(String filePath) {
        return NativeTestServerJni.get().getFileURL(sEmbeddedTestServerAdapter, filePath);
    }

    // Returns a URL that the server will return an Exabyte of data
    public static String getExabyteResponseURL() {
        return NativeTestServerJni.get().getExabyteResponseURL(sEmbeddedTestServerAdapter);
    }

    // The following URLs will make NativeTestServer serve a response based on
    // the contents of the corresponding file and its mock-http-headers file.

    public static String getSuccessURL() {
        return NativeTestServerJni.get().getFileURL(sEmbeddedTestServerAdapter, "/success.txt");
    }

    public static String getRedirectURL() {
        return NativeTestServerJni.get().getFileURL(sEmbeddedTestServerAdapter, "/redirect.html");
    }

    public static String getMultiRedirectURL() {
        return NativeTestServerJni.get()
                .getFileURL(sEmbeddedTestServerAdapter, "/multiredirect.html");
    }

    public static String getNotFoundURL() {
        return NativeTestServerJni.get().getFileURL(sEmbeddedTestServerAdapter, "/notfound.html");
    }

    public static String getServerErrorURL() {
        return NativeTestServerJni.get()
                .getFileURL(sEmbeddedTestServerAdapter, "/server_error.txt");
    }

    public static int getPort() {
        return NativeTestServerJni.get().getPort(sEmbeddedTestServerAdapter);
    }

    public static String getHostPort() {
        return NativeTestServerJni.get().getHostPort(sEmbeddedTestServerAdapter);
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
        NativeTestServerJni.get().registerRequestHandler(sEmbeddedTestServerAdapter, callback);
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
        @JniType("long")
        long create(
                @JniType("std::string") String filePath,
                @JniType("std::string") String testDataDir,
                @JniType("bool") boolean useHttps,
                @JniType("net::EmbeddedTestServer::ServerCertificate") @ServerCertificate
                        int certificate);

        void destroy(long nativeEmbeddedTestServerAdapter);

        void start(long nativeEmbeddedTestServerAdapter);

        void enableConnectProxy(
                long nativeEmbeddedTestServerAdapter,
                @JniType("std::vector<std::string>") String[] urls);

        @JniType("std::string")
        String getEchoBodyURL(long nativeEmbeddedTestServerAdapter);

        @JniType("std::string")
        String getEchoHeaderURL(
                long nativeEmbeddedTestServerAdapter, @JniType("std::string") String header);

        @JniType("std::string")
        String getEchoAllHeadersURL(long nativeEmbeddedTestServerAdapter);

        @JniType("std::string")
        String getEchoMethodURL(long nativeEmbeddedTestServerAdapter);

        @JniType("std::string")
        String getUseEncodingURL(
                long nativeEmbeddedTestServerAdapter, @JniType("std::string") String encoding);

        @JniType("std::string")
        String getRedirectToEchoBodyURL(long nativeEmbeddedTestServerAdapter);

        @JniType("std::string")
        String getFileURL(
                long nativeEmbeddedTestServerAdapter, @JniType("std::string") String filePath);

        @JniType("std::string")
        String getExabyteResponseURL(long nativeEmbeddedTestServerAdapter);

        @JniType("std::string")
        String getHostPort(long nativeEmbeddedTestServerAdapter);

        @JniType("int")
        int getPort(long nativeEmbeddedTestServerAdapter);

        void registerRequestHandler(
                long nativeEmbeddedTestServerAdapter,
                @JniType("std::unique_ptr<cronet::NativeTestServerHandleRequestCallback>")
                        HandleRequestCallback callback);
    }
}
