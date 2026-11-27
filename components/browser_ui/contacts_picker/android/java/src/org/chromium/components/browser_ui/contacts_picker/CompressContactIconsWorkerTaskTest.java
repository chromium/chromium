// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.contacts_picker;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertSame;
import static org.junit.Assert.assertTrue;

import android.content.res.Resources;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.graphics.Color;
import android.graphics.drawable.BitmapDrawable;

import androidx.core.graphics.drawable.RoundedBitmapDrawable;
import androidx.core.graphics.drawable.RoundedBitmapDrawableFactory;
import androidx.test.core.app.ApplicationProvider;

import org.junit.After;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.GraphicsMode;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.blink.mojom.ContactIconBlob;

import java.util.Collections;
import java.util.IdentityHashMap;
import java.util.List;
import java.util.Set;
import java.util.concurrent.CompletableFuture;
import java.util.concurrent.atomic.AtomicBoolean;
import java.util.concurrent.atomic.AtomicReference;

/** Tests for {@link CompressContactIconsWorkerTask}. */
@RunWith(BaseRobolectricTestRunner.class)
@GraphicsMode(GraphicsMode.Mode.NATIVE)
public class CompressContactIconsWorkerTaskTest {
    @After
    public void tearDown() {
        CompressContactIconsWorkerTask.sDisableForTesting = false;
    }

    @Test
    public void testCompressMultipleContactsAndSelfIcons() {
        Resources resources = ApplicationProvider.getApplicationContext().getResources();
        PickerCategoryView.ContactsBitmapCache bitmapCache =
                new PickerCategoryView.ContactsBitmapCache();

        // Start with a larger bitmap so the reused ByteArrayOutputStream buffer expands before
        // compressing smaller bitmaps in subsequent iterations.
        Bitmap largeBitmap = createPatternBitmap(64, 64, Color.RED, Color.WHITE);
        Bitmap smallBitmap = createPatternBitmap(8, 12, Color.BLUE, Color.YELLOW);
        Bitmap selfBitmapDrawableBitmap = createPatternBitmap(16, 16, Color.GREEN, Color.BLACK);
        Bitmap selfRoundedBitmap = createPatternBitmap(20, 20, Color.CYAN, Color.MAGENTA);

        ContactDetails contactLarge = new ContactDetails("1", "Contact Large", null, null, null);
        ContactDetails contactSmall = new ContactDetails("2", "Contact Small", null, null, null);
        ContactDetails contactNoIcon = new ContactDetails("3", "Contact NoIcon", null, null, null);
        ContactDetails contactSelfBitmap =
                new ContactDetails("4", "Self BitmapDrawable", null, null, null);
        ContactDetails contactSelfRounded =
                new ContactDetails("5", "Self RoundedBitmapDrawable", null, null, null);

        bitmapCache.putBitmap(contactLarge.getId(), largeBitmap);
        bitmapCache.putBitmap(contactSmall.getId(), smallBitmap);
        bitmapCache.putBitmap(contactNoIcon.getId(), null);

        contactSelfBitmap.setIsSelf(true);
        contactSelfBitmap.setSelfIcon(new BitmapDrawable(resources, selfBitmapDrawableBitmap));

        contactSelfRounded.setIsSelf(true);
        RoundedBitmapDrawable roundedDrawable =
                RoundedBitmapDrawableFactory.create(resources, selfRoundedBitmap);
        contactSelfRounded.setSelfIcon(roundedDrawable);

        List<ContactDetails> selectedContacts =
                List.of(
                        contactLarge,
                        contactSmall,
                        contactNoIcon,
                        contactSelfBitmap,
                        contactSelfRounded);

        AtomicReference<List<ContactDetails>> callbackContacts = new AtomicReference<>();
        CompressContactIconsWorkerTask task =
                new CompressContactIconsWorkerTask(
                        bitmapCache, selectedContacts, callbackContacts::set);

        runDoInBackgroundOffUiThread(task);
        task.onPostExecute(null);

        assertSame(selectedContacts, callbackContacts.get());
        assertTrue(contactNoIcon.getIcons().isEmpty());

        List<ContactDetails> compressedContacts =
                List.of(contactLarge, contactSmall, contactSelfBitmap, contactSelfRounded);
        List<Bitmap> expectedBitmaps =
                List.of(largeBitmap, smallBitmap, selfBitmapDrawableBitmap, selfRoundedBitmap);
        Set<byte[]> seenPayloads = Collections.newSetFromMap(new IdentityHashMap<>());
        for (int i = 0; i < compressedContacts.size(); i++) {
            ContactDetails contact = compressedContacts.get(i);
            assertEncodedPngMatchesBitmap(contact, expectedBitmaps.get(i));
            assertTrue(seenPayloads.add(contact.getIcons().get(0).data));
        }
    }

    @Test
    public void testDisableForTestingSkipsCompression() {
        PickerCategoryView.ContactsBitmapCache bitmapCache =
                new PickerCategoryView.ContactsBitmapCache();
        Bitmap bitmap = createPatternBitmap(16, 16, Color.RED, Color.WHITE);
        ContactDetails contact = new ContactDetails("1", "Contact One", null, null, null);
        bitmapCache.putBitmap(contact.getId(), bitmap);

        CompressContactIconsWorkerTask.sDisableForTesting = true;
        List<ContactDetails> selectedContacts = List.of(contact);
        AtomicBoolean callbackInvoked = new AtomicBoolean();
        CompressContactIconsWorkerTask task =
                new CompressContactIconsWorkerTask(
                        bitmapCache, selectedContacts, contacts -> callbackInvoked.set(true));

        runDoInBackgroundOffUiThread(task);
        task.onPostExecute(null);

        assertTrue(callbackInvoked.get());
        assertTrue(contact.getIcons().isEmpty());
    }

    private static Bitmap createPatternBitmap(
            int width, int height, int fillColor, int cornerColor) {
        Bitmap bitmap = Bitmap.createBitmap(width, height, Bitmap.Config.ARGB_8888);
        bitmap.eraseColor(fillColor);
        bitmap.setPixel(0, 0, cornerColor);
        bitmap.setPixel(width - 1, height - 1, cornerColor);
        return bitmap;
    }

    private static void runDoInBackgroundOffUiThread(CompressContactIconsWorkerTask task) {
        try {
            CompletableFuture.supplyAsync(task::doInBackground).get();
        } catch (Exception e) {
            throw new RuntimeException(e);
        }
    }

    private static void assertEncodedPngMatchesBitmap(
            ContactDetails contact, Bitmap expectedBitmap) {
        assertEquals(1, contact.getIcons().size());
        ContactIconBlob blob = contact.getIcons().get(0);
        assertEquals("image/png", blob.mimeType);
        assertNotNull(blob.data);

        Bitmap decoded = BitmapFactory.decodeByteArray(blob.data, 0, blob.data.length);
        assertNotNull(decoded);
        assertEquals(expectedBitmap.getWidth(), decoded.getWidth());
        assertEquals(expectedBitmap.getHeight(), decoded.getHeight());
        assertTrue(expectedBitmap.sameAs(decoded));
    }
}
