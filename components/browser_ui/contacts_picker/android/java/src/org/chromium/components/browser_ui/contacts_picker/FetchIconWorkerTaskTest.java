// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.contacts_picker;

import android.content.ContentResolver;
import android.database.Cursor;
import android.database.MatrixCursor;
import android.graphics.Bitmap;
import android.provider.ContactsContract;

import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mockito;

import org.chromium.base.Callback;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.build.annotations.Nullable;

import java.util.concurrent.CompletableFuture;

/** Tests for {@link FetchIconWorkerTask}. */
@RunWith(BaseRobolectricTestRunner.class)
public class FetchIconWorkerTaskTest {

    @After
    public void tearDown() {
        ContactsPickerImageDecoderJni.setInstanceForTesting(null);
    }

    private static ContentResolver createMockResolver(Cursor cursor) {
        ContentResolver resolver = Mockito.mock(ContentResolver.class);
        Mockito.when(
                        resolver.query(
                                Mockito.any(),
                                Mockito.any(),
                                Mockito.any(),
                                Mockito.any(),
                                Mockito.any()))
                .thenReturn(cursor);
        return resolver;
    }

    private static byte @Nullable [] runDoInBackgroundOffUiThread(FetchIconWorkerTask task) {
        try {
            return CompletableFuture.supplyAsync(task::doInBackground).get();
        } catch (Exception e) {
            throw new RuntimeException(e);
        }
    }

    @Test
    @SmallTest
    public void testFetchIconWorkerTask() {
        byte[] testBytes = new byte[] {10, 20, 30};
        Bitmap testBitmap =
                Bitmap.createBitmap(/* width= */ 36, /* height= */ 36, Bitmap.Config.ARGB_8888);

        MatrixCursor photoCursor =
                new MatrixCursor(new String[] {ContactsContract.Contacts.Photo.PHOTO});
        photoCursor.newRow().add(ContactsContract.Contacts.Photo.PHOTO, testBytes);

        ContentResolver resolver = createMockResolver(photoCursor);

        // Expected to be invoked by `onPostExecute` for valid photo bytes; simulates a
        // successful native decode returning `testBitmap`.
        ContactsPickerImageDecoderJni.setInstanceForTesting(
                new ContactsPickerImageDecoder.Natives() {
                    @Override
                    public void decodeImage(
                            byte[] data, int desiredSize, Callback<@Nullable Bitmap> callback) {
                        Assert.assertArrayEquals(testBytes, data);
                        Assert.assertEquals(36, desiredSize);
                        callback.onResult(testBitmap);
                    }
                });

        final Bitmap[] retrievedIcon = new Bitmap[1];
        final String[] retrievedId = new String[1];
        FetchIconWorkerTask task =
                new FetchIconWorkerTask(
                        /* id= */ "100",
                        resolver,
                        (icon, id) -> {
                            retrievedIcon[0] = icon;
                            retrievedId[0] = id;
                        });
        task.setDesiredIconSize(/* iconSize= */ 36);

        byte[] rawBytes = runDoInBackgroundOffUiThread(task);
        Assert.assertArrayEquals(testBytes, rawBytes);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    task.onPostExecute(rawBytes);
                });
        Assert.assertEquals(testBitmap, retrievedIcon[0]);
        Assert.assertEquals("100", retrievedId[0]);
    }

    @Test
    @SmallTest
    public void testFetchIconWorkerTaskCorruptedPayload() {
        byte[] corruptBytes = new byte[] {0x41, 0x42, 0x43, 0x44};

        MatrixCursor photoCursor =
                new MatrixCursor(new String[] {ContactsContract.Contacts.Photo.PHOTO});
        photoCursor.newRow().add(ContactsContract.Contacts.Photo.PHOTO, corruptBytes);

        ContentResolver resolver = createMockResolver(photoCursor);

        // Expected to be invoked by `onPostExecute`; simulates the native decoder failing on
        // corrupted image bytes and returning null.
        ContactsPickerImageDecoderJni.setInstanceForTesting(
                new ContactsPickerImageDecoder.Natives() {
                    @Override
                    public void decodeImage(
                            byte[] data, int desiredSize, Callback<@Nullable Bitmap> callback) {
                        Assert.assertArrayEquals(corruptBytes, data);
                        callback.onResult(null);
                    }
                });

        final boolean[] callbackInvoked = new boolean[1];
        final Bitmap[] retrievedIcon = new Bitmap[1];
        FetchIconWorkerTask task =
                new FetchIconWorkerTask(
                        /* id= */ "102",
                        resolver,
                        (icon, id) -> {
                            callbackInvoked[0] = true;
                            retrievedIcon[0] = icon;
                        });
        task.setDesiredIconSize(/* iconSize= */ 36);

        byte[] rawBytes = runDoInBackgroundOffUiThread(task);
        Assert.assertArrayEquals(corruptBytes, rawBytes);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    task.onPostExecute(rawBytes);
                });
        Assert.assertTrue(callbackInvoked[0]);
        Assert.assertNull(retrievedIcon[0]);
    }

    @Test
    @SmallTest
    public void testFetchIconWorkerTaskOversized() {
        byte[] largeBytes = new byte[ContactsPickerImageDecoder.MAX_IMAGE_SIZE_BYTES + 1];

        MatrixCursor photoCursor =
                new MatrixCursor(new String[] {ContactsContract.Contacts.Photo.PHOTO});
        photoCursor.newRow().add(ContactsContract.Contacts.Photo.PHOTO, largeBytes);

        ContentResolver resolver = createMockResolver(photoCursor);

        // Expected NOT to be invoked: `doInBackground` rejects blobs exceeding
        // `MAX_IMAGE_SIZE_BYTES` and returns null, so `onPostExecute` short-circuits before JNI.
        final boolean[] decoderCalled = new boolean[1];
        ContactsPickerImageDecoderJni.setInstanceForTesting(
                new ContactsPickerImageDecoder.Natives() {
                    @Override
                    public void decodeImage(
                            byte[] data, int desiredSize, Callback<@Nullable Bitmap> callback) {
                        decoderCalled[0] = true;
                        callback.onResult(null);
                    }
                });

        final Bitmap[] retrievedIcon = new Bitmap[1];
        FetchIconWorkerTask task =
                new FetchIconWorkerTask(
                        /* id= */ "101",
                        resolver,
                        (icon, id) -> {
                            retrievedIcon[0] = icon;
                        });

        byte[] rawBytes = runDoInBackgroundOffUiThread(task);
        Assert.assertNull(rawBytes);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    task.onPostExecute(rawBytes);
                });
        Assert.assertFalse(decoderCalled[0]);
        Assert.assertNull(retrievedIcon[0]);
    }

    @Test
    @SmallTest
    public void testFetchIconWorkerTaskEmptyBlob() {
        byte[] emptyBytes = new byte[0];

        MatrixCursor photoCursor =
                new MatrixCursor(new String[] {ContactsContract.Contacts.Photo.PHOTO});
        photoCursor.newRow().add(ContactsContract.Contacts.Photo.PHOTO, emptyBytes);

        ContentResolver resolver = createMockResolver(photoCursor);

        // Expected NOT to be invoked: `onPostExecute` short-circuits on 0-byte arrays before
        // calling `ContactsPickerImageDecoder.decodeImage`.
        final boolean[] decoderCalled = new boolean[1];
        ContactsPickerImageDecoderJni.setInstanceForTesting(
                new ContactsPickerImageDecoder.Natives() {
                    @Override
                    public void decodeImage(
                            byte[] data, int desiredSize, Callback<@Nullable Bitmap> callback) {
                        decoderCalled[0] = true;
                        callback.onResult(null);
                    }
                });

        final boolean[] callbackInvoked = new boolean[1];
        final Bitmap[] retrievedIcon = new Bitmap[1];
        FetchIconWorkerTask task =
                new FetchIconWorkerTask(
                        /* id= */ "103",
                        resolver,
                        (icon, id) -> {
                            callbackInvoked[0] = true;
                            retrievedIcon[0] = icon;
                        });

        byte[] rawBytes = runDoInBackgroundOffUiThread(task);
        Assert.assertArrayEquals(emptyBytes, rawBytes);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    task.onPostExecute(rawBytes);
                });
        Assert.assertFalse(decoderCalled[0]);
        Assert.assertTrue(callbackInvoked[0]);
        Assert.assertNull(retrievedIcon[0]);
    }

    @Test
    @SmallTest
    public void testFetchIconWorkerTaskCancelledBeforeDecodeCompletes() {
        byte[] testBytes = new byte[] {10, 20, 30};
        Bitmap testBitmap =
                Bitmap.createBitmap(/* width= */ 36, /* height= */ 36, Bitmap.Config.ARGB_8888);

        MatrixCursor photoCursor =
                new MatrixCursor(new String[] {ContactsContract.Contacts.Photo.PHOTO});
        photoCursor.newRow().add(ContactsContract.Contacts.Photo.PHOTO, testBytes);

        ContentResolver resolver = createMockResolver(photoCursor);

        // Expected to be invoked by `onPostExecute`; captures `callback` without running it
        // immediately so the test can cancel the task while decoding is in flight.
        @SuppressWarnings("unchecked")
        final Callback<@Nullable Bitmap>[] pendingDecoderCallback = new Callback[1];
        ContactsPickerImageDecoderJni.setInstanceForTesting(
                new ContactsPickerImageDecoder.Natives() {
                    @Override
                    public void decodeImage(
                            byte[] data, int desiredSize, Callback<@Nullable Bitmap> callback) {
                        pendingDecoderCallback[0] = callback;
                    }
                });

        final boolean[] callbackInvoked = new boolean[1];
        FetchIconWorkerTask task =
                new FetchIconWorkerTask(
                        /* id= */ "104",
                        resolver,
                        (icon, id) -> {
                            callbackInvoked[0] = true;
                        });
        task.setDesiredIconSize(/* iconSize= */ 36);

        byte[] rawBytes = runDoInBackgroundOffUiThread(task);
        Assert.assertArrayEquals(testBytes, rawBytes);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    task.onPostExecute(rawBytes);
                    Assert.assertNotNull(pendingDecoderCallback[0]);
                    // Cancel the task while the out-of-process decode is in flight.
                    task.cancel(/* mayInterruptIfRunning= */ true);
                    pendingDecoderCallback[0].onResult(testBitmap);
                });
        Assert.assertFalse(callbackInvoked[0]);
    }
}
