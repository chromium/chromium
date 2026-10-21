// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chromecast.shell;

import static org.junit.Assert.assertEquals;

import android.content.Intent;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.CommandLine;
import org.chromium.base.test.BaseRobolectricTestRunner;

import java.util.Arrays;

/** Tests for CastCommandLineHelper. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class CastCommandLineHelperTest {
    @Before
    public void setUp() {
        CommandLine.resetForTesting(false);
    }

    @Test
    public void testSetCommandLineArgs() {
        Intent intent = new Intent();
        CastCommandLineHelper.setCommandLineArgs(
                intent, new String[] {"--enable-cast-receiver", "--remote-debugging-port=9222"});

        String[] args =
                intent.getStringArrayExtra("com.google.android.apps.mediashell.CommandLineArgs");
        assertEquals(2, args.length);
        assertEquals("--enable-cast-receiver", args[0]);
        assertEquals("--remote-debugging-port=9222", args[1]);
    }

    @Test
    public void testInitCommandLineWithIntent_AllowedSwitch() {
        Intent intent = new Intent();
        CastCommandLineHelper.setCommandLineArgs(
                intent, new String[] {"--enable-cast-receiver", "--v=1"});
        CastCommandLineHelper.initCommandLine(intent);

        CommandLine commandLine = CommandLine.getInstance();
        assertEquals(true, commandLine.hasSwitch("enable-cast-receiver"));
        assertEquals("1", commandLine.getSwitchValue("v"));
    }

    @Test
    public void testInitCommandLineWithIntent_DisallowedSwitch() {
        Intent intent = new Intent();
        CastCommandLineHelper.setCommandLineArgs(
                intent, new String[] {"--disable-web-security", "--enable-cast-receiver"});
        CastCommandLineHelper.initCommandLine(intent);

        CommandLine commandLine = CommandLine.getInstance();
        assertEquals(false, commandLine.hasSwitch("disable-web-security"));
        assertEquals(true, commandLine.hasSwitch("enable-cast-receiver"));
    }

    @Test
    public void testInitCommandLineWithIntent_PositionalArg() {
        Intent intent = new Intent();
        CastCommandLineHelper.setCommandLineArgs(
                intent, new String[] {"https://malicious.com", "--enable-cast-receiver"});
        CastCommandLineHelper.initCommandLine(intent);

        CommandLine commandLine = CommandLine.getInstance();
        // Positional argument should be ignored.
        assertEquals(
                false,
                Arrays.asList(CommandLine.getJavaSwitchesForTesting())
                        .contains("https://malicious.com"));
        assertEquals(true, commandLine.hasSwitch("enable-cast-receiver"));
    }

    @Test
    public void testInitCommandLineWithIntent_InvalidSwitches() {
        Intent intent = new Intent();
        CastCommandLineHelper.setCommandLineArgs(
                intent, new String[] {"-", "--", "--=", "-=", "--enable-cast-receiver"});
        CastCommandLineHelper.initCommandLine(intent);

        CommandLine commandLine = CommandLine.getInstance();
        assertEquals(true, commandLine.hasSwitch("enable-cast-receiver"));
    }
}
