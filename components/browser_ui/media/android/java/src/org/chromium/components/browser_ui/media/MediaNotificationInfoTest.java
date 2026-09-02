// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.media;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.fail;

import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.services.media_session.MediaMetadata;

import java.util.HashSet;
import java.util.Set;

/** Robolectric tests for MediaImageManager. */
@RunWith(BaseRobolectricTestRunner.class)
public class MediaNotificationInfoTest {
    @Mock private MediaNotificationListener mListener;
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

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
        var _ = info.hashCode();
    }

    @Test
    public void testBuilderCopyConstructor() {
        MediaNotificationInfo.Builder builder = new MediaNotificationInfo.Builder();
        builder.setOrigin("https://example.com");
        builder.setMetadata(new MediaMetadata("title", "artist", "album"));
        builder.setListener(mListener);
        builder.setInstanceId(42);
        builder.setId(100);
        builder.setPaused(true);
        builder.setPrivate(false);
        builder.setNotificationSmallIcon(1);
        builder.setDefaultNotificationLargeIcon(2);

        builder.setActions(
                MediaNotificationInfo.ACTION_PLAY_PAUSE | MediaNotificationInfo.ACTION_STOP);
        Set<Integer> sessionActions = new HashSet<>();
        sessionActions.add(1);
        sessionActions.add(2);
        builder.setMediaSessionActions(sessionActions);

        MediaNotificationInfo original = builder.build();
        MediaNotificationInfo copy = new MediaNotificationInfo.Builder(original).build();

        assertNotNull(copy);
        assertEquals(original.origin, copy.origin);
        assertEquals(original.metadata.getTitle(), copy.metadata.getTitle());
        assertEquals(original.metadata.getArtist(), copy.metadata.getArtist());
        assertEquals(original.metadata.getAlbum(), copy.metadata.getAlbum());
        assertEquals(original.listener, copy.listener);
        assertEquals(original.instanceId, copy.instanceId);
        assertEquals(original.id, copy.id);
        assertEquals(original.isPaused, copy.isPaused);
        assertEquals(original.isPrivate, copy.isPrivate);
        assertEquals(original.notificationSmallIcon, copy.notificationSmallIcon);
        assertEquals(original.defaultNotificationLargeIcon, copy.defaultNotificationLargeIcon);

        assertEquals(original.supportsPlayPause(), copy.supportsPlayPause());
        assertEquals(original.supportsStop(), copy.supportsStop());
        assertEquals(original.mediaSessionActions, copy.mediaSessionActions);
    }

    @Test
    public void testBuilderWithSourceTitle() {
        MediaNotificationInfo.Builder builder = new MediaNotificationInfo.Builder();
        builder.setOrigin("https://example.com");
        builder.setMetadata(new MediaMetadata("title", "artist", "album", "example.com"));
        builder.setListener(mListener);
        builder.setInstanceId(0);
        builder.setId(0);

        MediaNotificationInfo info = builder.build();
        assertNotNull(info);
        assertEquals("https://example.com", info.origin);
        assertEquals("title", info.metadata.getTitle());
        assertEquals("artist", info.metadata.getArtist());
        assertEquals("album", info.metadata.getAlbum());
        assertEquals("example.com", info.metadata.getSourceTitle());
    }
}
