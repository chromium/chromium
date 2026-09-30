// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.contacts_picker;

import android.content.ContentResolver;
import android.database.Cursor;
import android.database.MatrixCursor;
import android.graphics.Bitmap;
import android.net.Uri;
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
import org.chromium.components.browser_ui.contacts_picker.PickerCategoryView.SystemContactsWorkerTask;
import org.chromium.payments.mojom.PaymentAddress;

import java.io.ByteArrayOutputStream;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;
import java.util.Random;

/** Tests for the {@link SystemContactsWorkerTask} class. */
@RunWith(BaseRobolectricTestRunner.class)
public class SystemContactsWorkerTaskTest {

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

    @Test
    @SmallTest
    public void testParsing() throws Exception {
        Uri sessionUri = Uri.parse("content://org.chromium.test.contacts/session/1");

        String[] columns =
                new String[] {
                    ContactsContract.Data.CONTACT_ID,
                    ContactsContract.Data.MIMETYPE,
                    ContactsContract.Data.DISPLAY_NAME_PRIMARY,
                    ContactsContract.Data.DATA1,
                    ContactsContract.CommonDataKinds.StructuredPostal.CITY,
                    ContactsContract.CommonDataKinds.StructuredPostal.COUNTRY,
                    ContactsContract.CommonDataKinds.StructuredPostal.POSTCODE,
                    ContactsContract.CommonDataKinds.StructuredPostal.REGION,
                    ContactsContract.CommonDataKinds.Photo.PHOTO
                };

        MatrixCursor cursor = new MatrixCursor(columns);

        // Contact 1: Name, Email, Phone
        cursor.newRow()
                .add(ContactsContract.Data.CONTACT_ID, "1")
                .add(
                        ContactsContract.Data.MIMETYPE,
                        ContactsContract.CommonDataKinds.StructuredName.CONTENT_ITEM_TYPE)
                .add(ContactsContract.Data.DISPLAY_NAME_PRIMARY, "Contact One");
        cursor.newRow()
                .add(ContactsContract.Data.CONTACT_ID, "1")
                .add(
                        ContactsContract.Data.MIMETYPE,
                        ContactsContract.CommonDataKinds.Email.CONTENT_ITEM_TYPE)
                .add(ContactsContract.Data.DISPLAY_NAME_PRIMARY, "Contact One")
                .add(ContactsContract.Data.DATA1, "one@example.com");
        cursor.newRow()
                .add(ContactsContract.Data.CONTACT_ID, "1")
                .add(
                        ContactsContract.Data.MIMETYPE,
                        ContactsContract.CommonDataKinds.Phone.CONTENT_ITEM_TYPE)
                .add(ContactsContract.Data.DISPLAY_NAME_PRIMARY, "Contact One")
                .add(ContactsContract.Data.DATA1, "555-1111");

        // Contact 2: Name, Address, Photo (PNG)
        cursor.newRow()
                .add(ContactsContract.Data.CONTACT_ID, "2")
                .add(
                        ContactsContract.Data.MIMETYPE,
                        ContactsContract.CommonDataKinds.StructuredName.CONTENT_ITEM_TYPE)
                .add(ContactsContract.Data.DISPLAY_NAME_PRIMARY, "Contact Two");

        cursor.newRow()
                .add(ContactsContract.Data.CONTACT_ID, "2")
                .add(
                        ContactsContract.Data.MIMETYPE,
                        ContactsContract.CommonDataKinds.StructuredPostal.CONTENT_ITEM_TYPE)
                .add(ContactsContract.Data.DISPLAY_NAME_PRIMARY, "Contact Two")
                .add(ContactsContract.Data.DATA1, "123 Street, Mountain View, USA")
                .add(ContactsContract.CommonDataKinds.StructuredPostal.CITY, "Mountain View")
                .add(ContactsContract.CommonDataKinds.StructuredPostal.COUNTRY, "USA")
                .add(ContactsContract.CommonDataKinds.StructuredPostal.POSTCODE, "94043")
                .add(ContactsContract.CommonDataKinds.StructuredPostal.REGION, "CA");

        Bitmap testBitmap =
                Bitmap.createBitmap(/* width= */ 1, /* height= */ 1, Bitmap.Config.ARGB_8888);
        ByteArrayOutputStream stream = new ByteArrayOutputStream();
        testBitmap.compress(Bitmap.CompressFormat.PNG, /* quality= */ 100, stream);
        byte[] pngData = stream.toByteArray();
        cursor.newRow()
                .add(ContactsContract.Data.CONTACT_ID, "2")
                .add(
                        ContactsContract.Data.MIMETYPE,
                        ContactsContract.CommonDataKinds.Photo.CONTENT_ITEM_TYPE)
                .add(ContactsContract.Data.DISPLAY_NAME_PRIMARY, "Contact Two")
                .add(ContactsContract.CommonDataKinds.Photo.PHOTO, pngData);

        // Expected to be invoked only for Contact "2" (which has `pngData`), and skipped for
        // Contact "1" (which has no photo).
        ContactsPickerImageDecoderJni.setInstanceForTesting(
                new ContactsPickerImageDecoder.Natives() {
                    @Override
                    public void decodeImage(
                            byte[] data, int desiredSize, Callback<@Nullable Bitmap> callback) {
                        Assert.assertArrayEquals(pngData, data);
                        callback.onResult(testBitmap);
                    }
                });

        ContentResolver resolver = createMockResolver(cursor);

        SystemContactsWorkerTask task = new SystemContactsWorkerTask(resolver, sessionUri);
        SystemContactsWorkerTask.RawResult rawResult = task.queryRawContacts();
        final SystemContactsWorkerTask.Result[] testResult = new SystemContactsWorkerTask.Result[1];
        final int[] callbackInvocations = new int[1];
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    task.decodeBitmapsAndComplete(
                            rawResult,
                            /* iconSize= */ 36,
                            (res) -> {
                                callbackInvocations[0]++;
                                testResult[0] = res;
                            });
                });
        Assert.assertEquals(1, callbackInvocations[0]);
        SystemContactsWorkerTask.Result result = testResult[0];
        Assert.assertNotNull(result);
        List<ContactDetails> contacts = result.contacts;

        Assert.assertEquals(2, contacts.size());

        ContactDetails c1 = contacts.get(0);
        Assert.assertEquals("1", c1.getId());
        Assert.assertEquals("Contact One", c1.getDisplayName());
        Assert.assertEquals(1, c1.getEmails().size());
        Assert.assertEquals("one@example.com", c1.getEmails().get(0));
        Assert.assertEquals(1, c1.getPhoneNumbers().size());
        Assert.assertEquals("555-1111", c1.getPhoneNumbers().get(0));

        ContactDetails c2 = contacts.get(1);
        Assert.assertEquals("2", c2.getId());
        Assert.assertEquals("Contact Two", c2.getDisplayName());
        Assert.assertEquals(1, c2.getAddresses().size());
        PaymentAddress addr = c2.getAddresses().get(0);
        Assert.assertEquals("Mountain View", addr.city);
        Assert.assertEquals("USA", addr.country);
        Assert.assertEquals("94043", addr.postalCode);
        Assert.assertEquals("CA", addr.region);
        Assert.assertEquals(1, addr.addressLine.length);
        Assert.assertEquals("123 Street, Mountain View, USA", addr.addressLine[0]);

        Assert.assertTrue(c2.getIcons().isEmpty());
        Assert.assertTrue(result.bitmaps.containsKey("1"));
        Assert.assertNull(result.bitmaps.get("1"));
        Assert.assertTrue(result.bitmaps.containsKey("2"));
        Assert.assertEquals(testBitmap, result.bitmaps.get("2"));
    }

    private static final String[] COLUMNS =
            new String[] {
                ContactsContract.Data.CONTACT_ID,
                ContactsContract.Data.MIMETYPE,
                ContactsContract.Data.DISPLAY_NAME_PRIMARY,
                ContactsContract.Data.DATA1,
                ContactsContract.CommonDataKinds.StructuredPostal.CITY,
                ContactsContract.CommonDataKinds.StructuredPostal.COUNTRY,
                ContactsContract.CommonDataKinds.StructuredPostal.POSTCODE,
                ContactsContract.CommonDataKinds.StructuredPostal.REGION,
                ContactsContract.CommonDataKinds.Photo.PHOTO
            };

    private static final String[] MIMETYPES =
            new String[] {
                ContactsContract.CommonDataKinds.StructuredName.CONTENT_ITEM_TYPE,
                ContactsContract.CommonDataKinds.Email.CONTENT_ITEM_TYPE,
                ContactsContract.CommonDataKinds.Phone.CONTENT_ITEM_TYPE,
                ContactsContract.CommonDataKinds.StructuredPostal.CONTENT_ITEM_TYPE,
                ContactsContract.CommonDataKinds.Photo.CONTENT_ITEM_TYPE,
                "unknown/mime-type",
                "",
                null
            };

    @Test
    @SmallTest
    public void testRandomMutations() {
        Random random = new Random(0xDEADBEEF);
        Uri sessionUri = Uri.parse("content://org.chromium.test.contacts/session/fuzz");
        byte[] oversizedPhoto = new byte[ContactsPickerImageDecoder.MAX_IMAGE_SIZE_BYTES + 1];

        for (int i = 0; i < 100; i++) {
            int rows = random.nextInt(50);
            MatrixCursor cursor = new MatrixCursor(COLUMNS);

            for (int r = 0; r < rows; r++) {
                String id = random.nextBoolean() ? String.valueOf(random.nextInt(100)) : null;
                String mimetype = MIMETYPES[random.nextInt(MIMETYPES.length)];
                String displayName = generateRandomString(random, /* maxLength= */ 20);
                String data1 = generateRandomString(random, /* maxLength= */ 50);

                String city = generateRandomString(random, /* maxLength= */ 15);
                String country = generateRandomString(random, /* maxLength= */ 10);
                String postcode = generateRandomString(random, /* maxLength= */ 8);
                String region = generateRandomString(random, /* maxLength= */ 15);

                byte[] photoBytes = null;
                if (random.nextInt(10) == 0) {
                    photoBytes = oversizedPhoto;
                } else if (random.nextBoolean()) {
                    photoBytes = new byte[random.nextInt(1000)];
                    random.nextBytes(photoBytes);
                }

                cursor.newRow()
                        .add(ContactsContract.Data.CONTACT_ID, id)
                        .add(ContactsContract.Data.MIMETYPE, mimetype)
                        .add(ContactsContract.Data.DISPLAY_NAME_PRIMARY, displayName)
                        .add(ContactsContract.Data.DATA1, data1)
                        .add(ContactsContract.CommonDataKinds.StructuredPostal.CITY, city)
                        .add(ContactsContract.CommonDataKinds.StructuredPostal.COUNTRY, country)
                        .add(ContactsContract.CommonDataKinds.StructuredPostal.POSTCODE, postcode)
                        .add(ContactsContract.CommonDataKinds.StructuredPostal.REGION, region)
                        .add(ContactsContract.CommonDataKinds.Photo.PHOTO, photoBytes);
            }

            ContentResolver resolver = createMockResolver(cursor);

            SystemContactsWorkerTask task = new SystemContactsWorkerTask(resolver, sessionUri);
            task.queryRawContacts();
        }
    }

    private static String generateRandomString(Random random, int maxLength) {
        if (random.nextBoolean()) return null;
        int length = random.nextInt(maxLength);
        StringBuilder sb = new StringBuilder(length);
        for (int i = 0; i < length; i++) {
            sb.append((char) (random.nextInt(96) + 32));
        }
        return sb.toString();
    }

    @Test
    @SmallTest
    public void testDecodeBitmapsAndCompleteMultipleContacts() {
        Bitmap testBitmap1 =
                Bitmap.createBitmap(/* width= */ 36, /* height= */ 36, Bitmap.Config.ARGB_8888);
        Bitmap testBitmap2 =
                Bitmap.createBitmap(/* width= */ 36, /* height= */ 36, Bitmap.Config.ARGB_8888);

        byte[] photo1Bytes = new byte[] {0x11, 0x22};
        byte[] photo2Bytes = new byte[] {0x33, 0x44};

        // Expected to be invoked for contacts with non-empty photo bytes ("c1" and "c3"), and
        // skipped for "c2" (null photo).
        ContactsPickerImageDecoderJni.setInstanceForTesting(
                new ContactsPickerImageDecoder.Natives() {
                    @Override
                    public void decodeImage(
                            byte[] data, int desiredSize, Callback<@Nullable Bitmap> callback) {
                        if (Arrays.equals(data, photo1Bytes)) {
                            callback.onResult(testBitmap1);
                        } else if (Arrays.equals(data, photo2Bytes)) {
                            callback.onResult(testBitmap2);
                        } else {
                            callback.onResult(null);
                        }
                    }
                });

        List<ContactDetails> contacts = new ArrayList<>();
        contacts.add(
                new ContactDetails(
                        /* id= */ "c1",
                        /* displayName= */ "Contact 1",
                        /* emails= */ null,
                        /* phoneNumbers= */ null,
                        /* addresses= */ null));
        contacts.add(
                new ContactDetails(
                        /* id= */ "c2",
                        /* displayName= */ "Contact 2",
                        /* emails= */ null,
                        /* phoneNumbers= */ null,
                        /* addresses= */ null));
        contacts.add(
                new ContactDetails(
                        /* id= */ "c3",
                        /* displayName= */ "Contact 3",
                        /* emails= */ null,
                        /* phoneNumbers= */ null,
                        /* addresses= */ null));

        Map<String, byte @Nullable []> rawPhotos = new LinkedHashMap<>();
        rawPhotos.put("c1", photo1Bytes);
        rawPhotos.put("c2", null);
        rawPhotos.put("c3", photo2Bytes);

        SystemContactsWorkerTask.RawResult rawResult =
                new SystemContactsWorkerTask.RawResult(contacts, rawPhotos);

        ContentResolver resolver = Mockito.mock(ContentResolver.class);
        SystemContactsWorkerTask task =
                new SystemContactsWorkerTask(
                        resolver, Uri.parse("content://test.contacts/session"));

        final SystemContactsWorkerTask.Result[] testResult = new SystemContactsWorkerTask.Result[1];
        final int[] invocations = new int[1];

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    task.decodeBitmapsAndComplete(
                            rawResult,
                            /* iconSize= */ 36,
                            (res) -> {
                                invocations[0]++;
                                testResult[0] = res;
                            });
                });

        Assert.assertEquals(1, invocations[0]);
        SystemContactsWorkerTask.Result result = testResult[0];
        Assert.assertNotNull(result);
        Assert.assertEquals(3, result.contacts.size());
        Assert.assertEquals(testBitmap1, result.bitmaps.get("c1"));
        Assert.assertNull(result.bitmaps.get("c2"));
        Assert.assertEquals(testBitmap2, result.bitmaps.get("c3"));
    }

    @Test
    @SmallTest
    public void testCorruptPhotoBlobDoesNotDropContacts() {
        Uri sessionUri = Uri.parse("content://org.chromium.test.contacts/session/corrupt");
        byte[] validPhoto = new byte[] {0x01, 0x02, 0x03};

        MatrixCursor cursor =
                new MatrixCursor(COLUMNS) {
                    @Override
                    public byte[] getBlob(int column) {
                        if (getPosition() == 0) {
                            throw new IllegalStateException("Simulated SQLiteBlobTooBigException");
                        }
                        return super.getBlob(column);
                    }
                };

        // Row 0: Contact 1 with a photo blob that throws when read.
        cursor.newRow()
                .add(ContactsContract.Data.CONTACT_ID, "1")
                .add(
                        ContactsContract.Data.MIMETYPE,
                        ContactsContract.CommonDataKinds.Photo.CONTENT_ITEM_TYPE)
                .add(ContactsContract.Data.DISPLAY_NAME_PRIMARY, "Contact One")
                .add(ContactsContract.CommonDataKinds.Photo.PHOTO, new byte[] {0x00});

        // Row 1: Contact 2 with a valid photo blob.
        cursor.newRow()
                .add(ContactsContract.Data.CONTACT_ID, "2")
                .add(
                        ContactsContract.Data.MIMETYPE,
                        ContactsContract.CommonDataKinds.Photo.CONTENT_ITEM_TYPE)
                .add(ContactsContract.Data.DISPLAY_NAME_PRIMARY, "Contact Two")
                .add(ContactsContract.CommonDataKinds.Photo.PHOTO, validPhoto);

        ContentResolver resolver = createMockResolver(cursor);
        SystemContactsWorkerTask task = new SystemContactsWorkerTask(resolver, sessionUri);
        SystemContactsWorkerTask.RawResult rawResult = task.queryRawContacts();

        Assert.assertEquals(2, rawResult.contacts.size());
        Assert.assertNull(rawResult.rawPhotos.get("1"));
        Assert.assertArrayEquals(validPhoto, rawResult.rawPhotos.get("2"));
    }
}
