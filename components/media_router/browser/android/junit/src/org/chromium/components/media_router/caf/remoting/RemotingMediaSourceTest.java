// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.media_router.caf.remoting;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNull;

import android.util.Base64;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;

/** Robolectric tests for RemotingMediaSource. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class RemotingMediaSourceTest {
    @Test
    public void testFrom() {
        assertNull(RemotingMediaSource.from("invalid-scheme"));
        assertNull(RemotingMediaSource.from("remote-playback:invalid-path"));
        assertNull(RemotingMediaSource.from("remote-playback:media-element?no-source=123"));

        String unencodedMediaUrl = "https://www.google.com";
        String sourceId =
                "remote-playback:media-element?source="
                        + Base64.encodeToString(unencodedMediaUrl.getBytes(), Base64.URL_SAFE)
                        + "&video_codec=vp8&audio_codec=mp3";
        RemotingMediaSource mediaSource = RemotingMediaSource.from(sourceId);
        assertEquals(sourceId, mediaSource.getSourceId());
        assertEquals(unencodedMediaUrl, mediaSource.getMediaUrl());
    }
}
