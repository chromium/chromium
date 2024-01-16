// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net;

import android.content.Context;

import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.base.test.util.UrlUtils;
import org.chromium.net.test.ServerCertificate;

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
        TestFilesInstaller.installIfNeeded(context);
        return NativeTestServerJni.get()
                .startNativeTestServer(
                        TestFilesInstaller.getInstalledPath(context),
                        UrlUtils.getIsolatedTestRoot(),
                        false, // useHttps
                        ServerCertificate.CERT_OK);
    }

    public static boolean startNativeTestServerWithHTTPS(
            Context context, @ServerCertificate int serverCertificate) {
        TestFilesInstaller.installIfNeeded(context);
        return NativeTestServerJni.get()
                .startNativeTestServer(
                        TestFilesInstaller.getInstalledPath(context),
                        UrlUtils.getIsolatedTestRoot(),
                        true, // useHttps
                        serverCertificate);
    }

    public static void shutdownNativeTestServer() {
        NativeTestServerJni.get().shutdownNativeTestServer();
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

    @NativeMethods("cronet_tests")
    interface Natives {
        boolean startNativeTestServer(
                String filePath,
                String testDataDir,
                boolean useHttps,
                @ServerCertificate int certificate);

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
    }
}
