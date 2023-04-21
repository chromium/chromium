// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chromecast.shell;

import static org.hamcrest.MatcherAssert.assertThat;
import static org.hamcrest.Matchers.containsString;
import static org.hamcrest.Matchers.not;
import static org.junit.Assert.assertEquals;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;

import java.io.BufferedReader;
import java.io.IOException;
import java.io.StringReader;
import java.util.Arrays;
import java.util.List;

// TODO(sandv): Add test cases as need arises.
/**
 * Tests for LogcatProvider.
 *
 * Full testing of elision of PII is done in LogcatElisionUnitTest.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class ElidedLogcatProviderUnitTest {
    private static final String LOG_FILE_NAME = "log.txt";

    private BufferedReader concatLines(List<String> contents) {
        StringBuilder sb = new StringBuilder();
        for (String s : contents) {
            sb.append(s);
            sb.append("\n");
        }
        return new BufferedReader(new StringReader(sb.toString()));
    }

    @Test
    public void testLogcatOfEmptyString() throws IOException {
        List<String> lines = Arrays.asList("");
        assertEquals("\n", ElidedLogcatProvider.elideLogcat(concatLines(lines)));
    }

    @Test
    public void testPreservesLines() throws IOException {
        List<String> lines = Arrays.asList("A", "B", "C");
        assertEquals("A\nB\nC\n", ElidedLogcatProvider.elideLogcat(concatLines(lines)));
    }

    @Test
    public void testElidesPii() throws IOException {
        List<String> lines = Arrays.asList("email me at someguy@mailservice.com",
                "file bugs at crbug.com", "at android.content.Intent", "at java.util.ArrayList",
                "mac address: AB-AB-AB-AB-AB-AB");
        String elided = ElidedLogcatProvider.elideLogcat(concatLines(lines));
        // PII like email addresses, web addresses, and MAC addresses are elided.
        assertThat(elided, not(containsString("someguy@mailservice.com")));
        assertThat(elided, not(containsString("crbug.com")));
        assertThat(elided, not(containsString("AB-AB-AB-AB-AB-AB")));
        // Tags for class names relevant for debugging should not be elided.
        assertThat(elided, containsString("android.content.Intent"));
        assertThat(elided, containsString("java.util.ArrayList"));
    }

    @Test
    public void testSpamRemoved() throws IOException {
        List<String> lines = Arrays.asList(
                "04-30 16:30:11.030 15721 15721 E libc    : Access denied finding property \"persist.mtk.mlog2logcat\"",
                "04-30 16:30:51.590 15721 15721 W MLOG_KERN: type=1400 audit(0.0:137421): avc: denied",
                "05-01 15:34:55.523   651   651 I chromium: [651:651:INFO:ccs_manager_impl.cc(891)] Waiting for all transports to be enabled");

        String elided = ElidedLogcatProvider.elideLogcat(concatLines(lines));
        assertThat(elided, not(containsString(lines.get(0))));
        assertThat(elided, not(containsString(lines.get(1))));
        assertThat(elided, containsString(lines.get(2)));
    }

    @Test
    public void testSpamBecomesEmptyString() throws IOException {
        List<String> lines = Arrays.asList(
                "04-30 16:30:11.030 15721 15721 E libc    : Access denied finding property \"persist.mtk.mlog2logcat\"");

        String elided = ElidedLogcatProvider.elideLogcat(concatLines(lines));
        assertEquals(elided, "");
    }
}
