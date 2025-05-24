// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.media;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.fail;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.services.media_session.MediaMetadata;

/** Robolectric tests for MediaImageManager. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class MediaNotificationInfoTest {
    @Mock private MediaNotificationListener mListener;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
    }

    @Test
    public void testEmptyBuilderDoesNotBuild() {
        MediaNotificationInfo.Builder builder = new MediaNotificationInfo.Builder();
        try {
            builder.build();
        } catch (AssertionError exception) {
            assertEquals("java.lang.AssertionError", exception.toString());
            return;
        }
        fail("Expected AssertionError");
    }

    @Test
    public void testBuilderWithOriginMetadataListener() {
        MediaNotificationInfo.Builder builder = new MediaNotificationInfo.Builder();
        builder.setOrigin("https://example.com");
        builder.setMetadata(new MediaMetadata("title", "artist", "album"));
        builder.setListener(mListener);

        try {
            builder.build();
        } catch (AssertionError exception) {
            assertEquals("java.lang.AssertionError", exception.toString());
            return;
        }
        fail("Expected AssertionError");
    }

    @Test
    public void testMinimalBuilder() {
        MediaNotificationInfo.Builder builder = new MediaNotificationInfo.Builder();
        builder.setOrigin("https://example.com");
        builder.setMetadata(new MediaMetadata("title", "artist", "album"));
        builder.setListener(mListener);
        builder.setInstanceId(0);
        builder.setId(0);

        MediaNotificationInfo info = builder.build();
        assertNotNull(info);
        assertEquals("https://example.com", info.origin);
        assertEquals("title", info.metadata.getTitle());
        assertEquals("artist", info.metadata.getArtist());
        assertEquals("album", info.metadata.getAlbum());
        assertEquals(mListener, info.listener);
        assertEquals(0, info.instanceId);
        assertEquals(0, info.id);
    }

    @Test
    public void testMinimalHashCode() {
        MediaNotificationInfo.Builder builder = new MediaNotificationInfo.Builder();
        builder.setOrigin("https://example.com");
        builder.setMetadata(new MediaMetadata("title", "artist", "album"));
        builder.setListener(mListener);
        builder.setInstanceId(0);
        builder.setId(0);

        MediaNotificationInfo info = builder.build();
        assertNotNull(info);

        // Make sure hashCode() doesn't crash.
        info.hashCode();
    }
}
