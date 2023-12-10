// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net.telemetry;

/** Utility class for grouping requests/responses into buckets */
public final class SizeBuckets {
    /**
     * Accepts a value in Bytes and groups it in buckets based on the Kilobyte value;
     *
     * @param sizeBytes size in Bytes
     * @return int bucket group
     */
    public static int calcRequestHeadersSizeBucket(long sizeBytes) {
        checkSizeIsValid(sizeBytes, "Request header size is negative");

        double sizeKB = sizeBytes / 1024.0;

        if (isInClosedOpenRange(sizeKB, 0, 1)) {
            return CronetStatsLog
                    .CRONET_TRAFFIC_REPORTED__REQUEST_HEADERS_SIZE__REQUEST_HEADERS_SIZE_BUCKET_UNDER_ONE_KIB;
        } else if (isInClosedOpenRange(sizeKB, 1, 10)) {
            return CronetStatsLog
                    .CRONET_TRAFFIC_REPORTED__REQUEST_HEADERS_SIZE__REQUEST_HEADERS_SIZE_BUCKET_ONE_TO_TEN_KIB;
        } else if (isInClosedOpenRange(sizeKB, 10, 25)) {
            return CronetStatsLog
                    .CRONET_TRAFFIC_REPORTED__REQUEST_HEADERS_SIZE__REQUEST_HEADERS_SIZE_BUCKET_TEN_TO_TWENTY_FIVE_KIB;
        } else if (isInClosedOpenRange(sizeKB, 25, 50)) {
            return CronetStatsLog
                    .CRONET_TRAFFIC_REPORTED__REQUEST_HEADERS_SIZE__REQUEST_HEADERS_SIZE_BUCKET_TWENTY_FIVE_TO_FIFTY_KIB;
        } else if (isInClosedOpenRange(sizeKB, 50, 100)) {
            return CronetStatsLog
                    .CRONET_TRAFFIC_REPORTED__REQUEST_HEADERS_SIZE__REQUEST_HEADERS_SIZE_BUCKET_FIFTY_TO_HUNDRED_KIB;
        } else { // sizeKB >= 100
            return CronetStatsLog
                    .CRONET_TRAFFIC_REPORTED__REQUEST_HEADERS_SIZE__REQUEST_HEADERS_SIZE_BUCKET_OVER_HUNDRED_KIB;
        }
    }

    /**
     * Accepts a value in Bytes and groups it in buckets based on the Kilobyte value;
     *
     * @param sizeBytes size in Bytes
     * @return int bucket group
     */
    public static int calcResponseHeadersSizeBucket(long sizeBytes) {
        checkSizeIsValid(sizeBytes, "Response header size is negative");

        double sizeKB = sizeBytes / 1024.0;

        if (isInClosedOpenRange(sizeKB, 0, 1)) {
            return CronetStatsLog
                    .CRONET_TRAFFIC_REPORTED__RESPONSE_HEADERS_SIZE__RESPONSE_HEADERS_SIZE_BUCKET_UNDER_ONE_KIB;
        } else if (isInClosedOpenRange(sizeKB, 1, 10)) {
            return CronetStatsLog
                    .CRONET_TRAFFIC_REPORTED__RESPONSE_HEADERS_SIZE__RESPONSE_HEADERS_SIZE_BUCKET_ONE_TO_TEN_KIB;
        } else if (isInClosedOpenRange(sizeKB, 10, 25)) {
            return CronetStatsLog
                    .CRONET_TRAFFIC_REPORTED__RESPONSE_HEADERS_SIZE__RESPONSE_HEADERS_SIZE_BUCKET_TEN_TO_TWENTY_FIVE_KIB;
        } else if (isInClosedOpenRange(sizeKB, 25, 50)) {
            return CronetStatsLog
                    .CRONET_TRAFFIC_REPORTED__RESPONSE_HEADERS_SIZE__RESPONSE_HEADERS_SIZE_BUCKET_TWENTY_FIVE_TO_FIFTY_KIB;
        } else if (isInClosedOpenRange(sizeKB, 50, 100)) {
            return CronetStatsLog
                    .CRONET_TRAFFIC_REPORTED__RESPONSE_HEADERS_SIZE__RESPONSE_HEADERS_SIZE_BUCKET_FIFTY_TO_HUNDRED_KIB;
        } else { // sizeKB >= 100
            return CronetStatsLog
                    .CRONET_TRAFFIC_REPORTED__RESPONSE_HEADERS_SIZE__RESPONSE_HEADERS_SIZE_BUCKET_OVER_HUNDRED_KIB;
        }
    }

    /**
     * Accepts a value in Bytes and groups it in buckets based on the Kilobyte value;
     *
     * @param sizeBytes size in Bytes
     * @return int bucket group
     */
    public static int calcRequestBodySizeBucket(long sizeBytes) {
        checkSizeIsValid(sizeBytes, "Request body size is negative");

        double sizeKB = sizeBytes / 1024.0;

        if (sizeKB == 0) {
            return CronetStatsLog
                    .CRONET_TRAFFIC_REPORTED__REQUEST_BODY_SIZE__REQUEST_BODY_SIZE_BUCKET_ZERO;
        } else if (sizeKB > 0 && sizeKB < 10) {
            return CronetStatsLog
                    .CRONET_TRAFFIC_REPORTED__REQUEST_BODY_SIZE__REQUEST_BODY_SIZE_BUCKET_UNDER_TEN_KIB;
        } else if (isInClosedOpenRange(sizeKB, 10, 50)) {
            return CronetStatsLog
                    .CRONET_TRAFFIC_REPORTED__REQUEST_BODY_SIZE__REQUEST_BODY_SIZE_BUCKET_TEN_TO_FIFTY_KIB;
        } else if (isInClosedOpenRange(sizeKB, 50, 200)) {
            return CronetStatsLog
                    .CRONET_TRAFFIC_REPORTED__REQUEST_BODY_SIZE__REQUEST_BODY_SIZE_BUCKET_FIFTY_TO_TWO_HUNDRED_KIB;
        } else if (isInClosedOpenRange(sizeKB, 200, 500)) {
            return CronetStatsLog
                    .CRONET_TRAFFIC_REPORTED__REQUEST_BODY_SIZE__REQUEST_BODY_SIZE_BUCKET_TWO_HUNDRED_TO_FIVE_HUNDRED_KIB;
        } else if (isInClosedOpenRange(sizeKB, 500, 1000)) {
            return CronetStatsLog
                    .CRONET_TRAFFIC_REPORTED__REQUEST_BODY_SIZE__REQUEST_BODY_SIZE_BUCKET_FIVE_HUNDRED_KIB_TO_ONE_MIB;
        } else if (isInClosedOpenRange(sizeKB, 1000, 5000)) {
            return CronetStatsLog
                    .CRONET_TRAFFIC_REPORTED__REQUEST_BODY_SIZE__REQUEST_BODY_SIZE_BUCKET_ONE_TO_FIVE_MIB;
        } else { // sizeKB >= 5000
            return CronetStatsLog
                    .CRONET_TRAFFIC_REPORTED__REQUEST_BODY_SIZE__REQUEST_BODY_SIZE_BUCKET_OVER_FIVE_MIB;
        }
    }

    /**
     * Accepts a value in Bytes and groups it in buckets based on the Kilobyte value;
     *
     * @param sizeBytes size in Bytes
     * @return int bucket group
     */
    public static int calcResponseBodySizeBucket(long sizeBytes) {
        checkSizeIsValid(sizeBytes, "Response body size is negative");

        double sizeKB = sizeBytes / 1024.0;

        if (sizeKB == 0) {
            return CronetStatsLog
                    .CRONET_TRAFFIC_REPORTED__RESPONSE_BODY_SIZE__RESPONSE_BODY_SIZE_BUCKET_ZERO;
        } else if (sizeKB > 0 && sizeKB < 10) {
            return CronetStatsLog
                    .CRONET_TRAFFIC_REPORTED__RESPONSE_BODY_SIZE__RESPONSE_BODY_SIZE_BUCKET_UNDER_TEN_KIB;
        } else if (isInClosedOpenRange(sizeKB, 10, 50)) {
            return CronetStatsLog
                    .CRONET_TRAFFIC_REPORTED__RESPONSE_BODY_SIZE__RESPONSE_BODY_SIZE_BUCKET_TEN_TO_FIFTY_KIB;
        } else if (isInClosedOpenRange(sizeKB, 50, 200)) {
            return CronetStatsLog
                    .CRONET_TRAFFIC_REPORTED__RESPONSE_BODY_SIZE__RESPONSE_BODY_SIZE_BUCKET_FIFTY_TO_TWO_HUNDRED_KIB;
        } else if (isInClosedOpenRange(sizeKB, 200, 500)) {
            return CronetStatsLog
                    .CRONET_TRAFFIC_REPORTED__RESPONSE_BODY_SIZE__RESPONSE_BODY_SIZE_BUCKET_TWO_HUNDRED_TO_FIVE_HUNDRED_KIB;
        } else if (isInClosedOpenRange(sizeKB, 500, 1000)) {
            return CronetStatsLog
                    .CRONET_TRAFFIC_REPORTED__RESPONSE_BODY_SIZE__RESPONSE_BODY_SIZE_BUCKET_FIVE_HUNDRED_KIB_TO_ONE_MIB;
        } else if (isInClosedOpenRange(sizeKB, 1000, 5000)) {
            return CronetStatsLog
                    .CRONET_TRAFFIC_REPORTED__RESPONSE_BODY_SIZE__RESPONSE_BODY_SIZE_BUCKET_ONE_TO_FIVE_MIB;
        } else {
            return CronetStatsLog
                    .CRONET_TRAFFIC_REPORTED__RESPONSE_BODY_SIZE__RESPONSE_BODY_SIZE_BUCKET_OVER_FIVE_MIB;
        }
    }

    private static void checkSizeIsValid(long sizeBytes, String errMessage) {
        if (sizeBytes < 0) {
            throw new IllegalArgumentException(errMessage);
        }
    }

    private static boolean isInClosedOpenRange(double value, int lowerBound, int upperBound) {
        return value >= lowerBound && value < upperBound;
    }

    private SizeBuckets() {}
}
