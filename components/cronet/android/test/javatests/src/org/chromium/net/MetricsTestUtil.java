// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net;

import static com.google.common.truth.Truth.assertThat;
import static com.google.common.truth.Truth.assertWithMessage;

import org.chromium.net.CronetTestRule.CronetImplementation;

import java.util.Date;
import java.util.LinkedList;
import java.util.NoSuchElementException;
import java.util.concurrent.Executor;

/**
 * Classes which are useful for testing Cronet's metrics implementation and are needed in more than
 * one test file.
 */
public class MetricsTestUtil {
    /** Executor which runs tasks only when told to with runAllTasks(). */
    public static class TestExecutor implements Executor {
        private final LinkedList<Runnable> mTaskQueue = new LinkedList<Runnable>();

        @Override
        public void execute(Runnable task) {
            mTaskQueue.add(task);
        }

        public void runAllTasks() {
            try {
                while (mTaskQueue.size() > 0) {
                    mTaskQueue.remove().run();
                }
            } catch (NoSuchElementException e) {
                throw new RuntimeException("Task was removed during iteration", e);
            }
        }
    }

    // Helper method to assert date1 is equals to or after date2.
    // Some implementation of java.util.Date broke the symmetric property, so
    // check both directions.
    public static void assertAfter(Date date1, Date date2) {
        assertWithMessage("date1: " + date1.getTime() + ", date2: " + date2.getTime())
                .that(date1.after(date2) || date1.equals(date2) || date2.equals(date1))
                .isTrue();
    }

    /**
     * Check existence of all the timing metrics that apply to most test requests, except those that
     * come from net::LoadTimingInfo::ConnectTiming. Also check some timing differences, focusing on
     * things we can't check with asserts in the CronetMetrics constructor. Don't check push times
     * here.
     */
    public static void checkTimingMetrics(
            CronetImplementation implementationUnderTest,
            RequestFinishedInfo.Metrics metrics,
            Date startTime,
            Date endTime) {
        if (implementationUnderTest == CronetImplementation.AOSP_PLATFORM) {
            // RequestFinishedInfoListener HttpEngineWrapper implementation has placeholder ie null
            // metrics. Don't bother checking timing metrics for AOSP whether it passes or not.
            return;
        }
        assertThat(metrics.getRequestStart()).isNotNull();
        assertAfter(metrics.getRequestStart(), startTime);
        assertThat(metrics.getSendingStart()).isNotNull();
        assertAfter(metrics.getSendingStart(), startTime);
        assertThat(metrics.getSendingEnd()).isNotNull();
        assertAfter(endTime, metrics.getSendingEnd());
        assertThat(metrics.getResponseStart()).isNotNull();
        assertAfter(metrics.getResponseStart(), startTime);
        assertThat(metrics.getRequestEnd()).isNotNull();
        assertAfter(endTime, metrics.getRequestEnd());
        assertAfter(metrics.getRequestEnd(), metrics.getRequestStart());
    }

    /**
     * Check that the timing metrics which come from net::LoadTimingInfo::ConnectTiming exist,
     * except SSL times in the case of non-https requests.
     */
    public static void checkHasConnectTiming(
            CronetImplementation implementationUnderTest,
            RequestFinishedInfo.Metrics metrics,
            Date startTime,
            Date endTime,
            boolean isSsl) {
        if (implementationUnderTest == CronetImplementation.AOSP_PLATFORM) {
            // RequestFinishedInfoListener HttpEngineWrapper implementation has placeholder ie null
            // metrics. Don't bother checking timing metrics for AOSP whether it passes or not.
            return;
        }
        assertThat(metrics.getDnsStart()).isNotNull();
        assertAfter(metrics.getDnsStart(), startTime);
        assertThat(metrics.getDnsEnd()).isNotNull();
        assertAfter(endTime, metrics.getDnsEnd());
        assertThat(metrics.getConnectStart()).isNotNull();
        assertAfter(metrics.getConnectStart(), startTime);
        assertThat(metrics.getConnectEnd()).isNotNull();
        assertAfter(endTime, metrics.getConnectEnd());
        if (isSsl) {
            assertThat(metrics.getSslStart()).isNotNull();
            assertAfter(metrics.getSslStart(), startTime);
            assertThat(metrics.getSslEnd()).isNotNull();
            assertAfter(endTime, metrics.getSslEnd());
        } else {
            assertThat(metrics.getSslStart()).isNull();
            assertThat(metrics.getSslEnd()).isNull();
        }
    }

    /** Check that the timing metrics from net::LoadTimingInfo::ConnectTiming don't exist. */
    public static void checkNoConnectTiming(
            CronetImplementation implementationUnderTest, RequestFinishedInfo.Metrics metrics) {
        if (implementationUnderTest == CronetImplementation.AOSP_PLATFORM) {
            // RequestFinishedInfoListener HttpEngineWrapper implementation has placeholder ie null
            // metrics. Although the checks below would pass, generally, don't bother checking
            // timing metrics for AOSP whether it passes or not.
            return;
        }
        assertThat(metrics.getDnsStart()).isNull();
        assertThat(metrics.getDnsEnd()).isNull();
        assertThat(metrics.getSslStart()).isNull();
        assertThat(metrics.getSslEnd()).isNull();
        assertThat(metrics.getConnectStart()).isNull();
        assertThat(metrics.getConnectEnd()).isNull();
    }

    /**
     * Check that RequestFinishedInfo looks the way it should look for a normal successful request.
     */
    public static void checkRequestFinishedInfo(
            CronetImplementation implementationUnderTest,
            RequestFinishedInfo info,
            String url,
            Date startTime,
            Date endTime) {
        assertWithMessage("RequestFinishedInfo.Listener must be called").that(info).isNotNull();
        assertThat(info.getUrl()).isEqualTo(url);
        assertThat(info.getResponseInfo()).isNotNull();
        assertThat(info.getException()).isNull();
        RequestFinishedInfo.Metrics metrics = info.getMetrics();
        assertWithMessage("RequestFinishedInfo.getMetrics() must not be null")
                .that(metrics)
                .isNotNull();

        // RequestFinishedInfoListener HttpEngineWrapper implementation has placeholder ie null
        // metrics. Don't bother checking timing metrics for AOSP whether it passes or not.
        if (implementationUnderTest != CronetImplementation.AOSP_PLATFORM) {
            // Check old (deprecated) timing metrics
            assertThat(metrics.getTotalTimeMs()).isAtLeast(0L);
            assertThat(metrics.getTotalTimeMs()).isAtLeast(metrics.getTtfbMs());
            // Check new timing metrics
            checkTimingMetrics(implementationUnderTest, metrics, startTime, endTime);
            assertThat(metrics.getPushStart()).isNull();
            assertThat(metrics.getPushEnd()).isNull();
            // Check data use metrics
            assertThat(metrics.getSentByteCount()).isGreaterThan(0L);
            assertThat(metrics.getReceivedByteCount()).isGreaterThan(0L);
        }
    }
}
