// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.minidump_uploader;

import org.chromium.components.minidump_uploader.util.HttpURLConnectionFactory;
import org.chromium.components.minidump_uploader.util.HttpURLConnectionFactoryImpl;

import java.io.BufferedReader;
import java.io.ByteArrayOutputStream;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileReader;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.net.HttpURLConnection;
import java.util.zip.GZIPOutputStream;

/**
 * This class tries to upload a minidump to the crash server.
 *
 * Minidumps are stored in multipart MIME format ready to form the body of a POST request. The MIME
 * boundary forms the first line of the file.
 */
public class MinidumpUploader {
    /* package */
    static final String CRASH_URL_STRING = "https://clients2.google.com/cr/report";
    /* package */
    static final String CONTENT_TYPE_TMPL = "multipart/form-data; boundary=%s";

    private final HttpURLConnectionFactory mHttpURLConnectionFactory;

    /**
     * The result of an upload attempt.
     *
     * An upload attempt may succeed, in which case the result message is the upload ID.
     * Alternatively it may fail either as a result of either a local or a remote error.
     */
    public static final class Result {
        private final int mErrorCode;
        private final String mResult;

        private Result(int errorCode, String result) {
            mErrorCode = errorCode;
            mResult = result;
        }

        /** Returns true if this result represents a succesful upload. */
        public boolean isSuccess() {
            return mErrorCode == 0;
        }

        /** Returns true if this result represents a remote error. */
        public boolean isUploadError() {
            return mErrorCode > 0;
        }

        /** Returns true if this result represents a local error. */
        public boolean isFailure() {
            return mErrorCode < 0;
        }

        /**
         * Returns the upload error code.
         *
         * @return 0 on success
         * @return <0 on local error
         * @return HTTP status code on remote error
         */
        public int errorCode() {
            return mErrorCode;
        }

        /**
         * The message associated with this result.
         *
         * @return the remotely assigned upload id, on success
         * @return descriptive error text otherwise.
         */
        public String message() {
            return mResult;
        }

        static Result failure(String result) {
            return new Result(-1, result);
        }

        static Result uploadError(int status, String result) {
            assert status > 0;
            return new Result(status, result);
        }

        static Result success(String result) {
            return new Result(0, result);
        }
    }

    public MinidumpUploader() {
        this(new HttpURLConnectionFactoryImpl());
    }

    public MinidumpUploader(HttpURLConnectionFactory httpURLConnectionFactory) {
        mHttpURLConnectionFactory = httpURLConnectionFactory;
    }

    /**
     * Attempt to upload a single file to the crash server.
     *
     * The result of the upload attempt is either success (and an associated report ID), or failure.
     * Failure may occur locally (the file is invalid or the network connection could not be
     * created) or remotely (the crash server rejected the upload with a HTTP error).
     *
     * @param fileToUpload the file containing a MIME-body with an attached minidump.
     * @return the success/failure result of the upload attempt.
     */
    public Result upload(File fileToUpload) {
        try {
            if (fileToUpload == null || !fileToUpload.exists()) {
                return Result.failure("Crash report does not exist");
            }
            HttpURLConnection connection =
                    mHttpURLConnectionFactory.createHttpURLConnection(CRASH_URL_STRING);
            if (connection == null) {
                return Result.failure("Failed to create connection");
            }
            configureConnectionForHttpPost(connection, readBoundary(fileToUpload));

            try (InputStream minidumpInputStream = new FileInputStream(fileToUpload);
                    OutputStream requestBodyStream =
                            new GZIPOutputStream(connection.getOutputStream())) {
                streamCopy(minidumpInputStream, requestBodyStream);
                int responseCode = connection.getResponseCode();
                // The crash server returns the crash ID in the response body.
                String responseContent = getResponseContentAsString(connection);
                String uploadId = responseContent != null ? responseContent : "unknown";
                if (isSuccessful(responseCode)) {
                    return Result.success(uploadId);
                } else {
                    // Return the remote error code and message.
                    return Result.uploadError(
                            responseCode,
                            connection.getResponseMessage() + " uploadId: " + uploadId);
                }
            } finally {
                connection.disconnect();
            }
        } catch (IOException | RuntimeException e) {
            return Result.failure(e.toString());
        }
    }

    /**
     * Configures a HttpURLConnection to send a HTTP POST request for uploading the minidump.
     *
     * This also reads the content-type from the minidump file.
     *
     * @param connection the HttpURLConnection to configure
     * @param boundary the MIME boundary used in the request body
     */
    private void configureConnectionForHttpPost(HttpURLConnection connection, String boundary) {
        connection.setDoOutput(true);
        connection.setRequestProperty("Connection", "Keep-Alive");
        connection.setRequestProperty("Content-Encoding", "gzip");
        connection.setRequestProperty("Content-Type", String.format(CONTENT_TYPE_TMPL, boundary));
    }

    /**
     * Get the MIME boundary from the file, for inclusion in Content-Type header.
     *
     * @return the MIME boundary used in the file.
     * @throws IOException if fileToUpload cannot be read
     * @throws RuntimeException if the MIME boundary is missing or malformed.
     */
    private String readBoundary(File fileToUpload) throws IOException {
        try (FileReader fileReader = new FileReader(fileToUpload);
                BufferedReader reader = new BufferedReader(fileReader)) {
            String boundary = reader.readLine();
            if (boundary == null || boundary.trim().isEmpty()) {
                throw new RuntimeException("File does not have a MIME boundary");
            }
            boundary = boundary.trim();
            if (!boundary.startsWith("--") || boundary.length() < 10) {
                throw new RuntimeException("File does not have a MIME boundary");
            }
            // Note: The regex allows all alphanumeric characters, as well as dashes.
            // This matches the code that generates minidumps boundaries:
            // https://chromium.googlesource.com/crashpad/crashpad/+/0c322ecc3f711c34fbf85b2cbe69f38b8dbccf05/util/net/http_multipart_builder.cc#36
            if (!boundary.matches("^[a-zA-Z0-9-]*$")) {
                throw new RuntimeException("File has an illegal MIME boundary: " + boundary);
            }
            boundary = boundary.substring(2); // Remove the initial --
            return boundary;
        }
    }

    /**
     * Returns whether the response code indicates a successful HTTP request.
     *
     * @param responseCode the response code
     * @return true if response code indicates success, false otherwise.
     */
    private boolean isSuccessful(int responseCode) {
        return responseCode == 200 || responseCode == 201 || responseCode == 202;
    }

    /**
     * Reads the response from |connection| as a String.
     *
     * @param connection the connection to read the response from.
     * @return the content of the response.
     */
    private String getResponseContentAsString(HttpURLConnection connection) throws IOException {
        ByteArrayOutputStream baos = new ByteArrayOutputStream();
        streamCopy(connection.getInputStream(), baos);
        if (baos.size() == 0) {
            return null;
        }
        return baos.toString();
    }

    /**
     * Copies all available data from |inStream| to |outStream|. Closes both streams when done.
     *
     * @param inStream the stream to read
     * @param outStream the stream to write to
     */
    private void streamCopy(InputStream inStream, OutputStream outStream) throws IOException {
        byte[] temp = new byte[4096];
        int bytesRead = inStream.read(temp);
        while (bytesRead >= 0) {
            outStream.write(temp, 0, bytesRead);
            bytesRead = inStream.read(temp);
        }
        inStream.close();
        outStream.close();
    }
}
