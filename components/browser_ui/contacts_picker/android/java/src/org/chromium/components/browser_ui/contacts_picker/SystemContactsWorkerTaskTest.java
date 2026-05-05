// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.contacts_picker;

import android.database.Cursor;
import android.database.MatrixCursor;
import android.net.Uri;
import android.provider.ContactsContract;
import android.test.mock.MockContentProvider;
import android.test.mock.MockContentResolver;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.components.browser_ui.contacts_picker.PickerCategoryView.SystemContactsWorkerTask;
import org.chromium.payments.mojom.PaymentAddress;

import java.util.List;

/** Tests for the SystemContactsWorkerTask class. */
@RunWith(BaseJUnit4ClassRunner.class)
@Batch(Batch.UNIT_TESTS)
public class SystemContactsWorkerTaskTest {

    private static class FakeContactProvider extends MockContentProvider {
        private final Cursor mCursor;

        public FakeContactProvider(Cursor cursor) {
            mCursor = cursor;
        }

        @Override
        public Cursor query(
                Uri uri,
                String[] projection,
                String selection,
                String[] selectionArgs,
                String sortOrder) {
            return mCursor;
        }
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

        android.graphics.Bitmap testBitmap =
                android.graphics.Bitmap.createBitmap(
                        1, 1, android.graphics.Bitmap.Config.ARGB_8888);
        java.io.ByteArrayOutputStream stream = new java.io.ByteArrayOutputStream();
        testBitmap.compress(android.graphics.Bitmap.CompressFormat.PNG, 100, stream);
        byte[] pngData = stream.toByteArray();
        cursor.newRow()
                .add(ContactsContract.Data.CONTACT_ID, "2")
                .add(
                        ContactsContract.Data.MIMETYPE,
                        ContactsContract.CommonDataKinds.Photo.CONTENT_ITEM_TYPE)
                .add(ContactsContract.Data.DISPLAY_NAME_PRIMARY, "Contact Two")
                .add(ContactsContract.CommonDataKinds.Photo.PHOTO, pngData);

        MockContentResolver resolver = new MockContentResolver();
        FakeContactProvider provider = new FakeContactProvider(cursor);
        resolver.addProvider(sessionUri.getAuthority(), provider);

        SystemContactsWorkerTask task = new SystemContactsWorkerTask(resolver, sessionUri);
        PickerCategoryView.SystemContactsWorkerTask.Result result = task.call();
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
        Assert.assertTrue(result.bitmaps.containsKey("2"));
        Assert.assertNotNull(result.bitmaps.get("2"));
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
        java.util.Random random = new java.util.Random(0xDEADBEEF);
        Uri sessionUri = Uri.parse("content://org.chromium.test.contacts/session/fuzz");

        for (int i = 0; i < 1000; i++) {
            int rows = random.nextInt(50);
            MatrixCursor cursor = new MatrixCursor(COLUMNS);

            for (int r = 0; r < rows; r++) {
                String id = random.nextBoolean() ? String.valueOf(random.nextInt(100)) : null;
                String mimetype = MIMETYPES[random.nextInt(MIMETYPES.length)];
                String displayName = generateRandomString(random, 20);
                String data1 = generateRandomString(random, 50);

                String city = generateRandomString(random, 15);
                String country = generateRandomString(random, 10);
                String postcode = generateRandomString(random, 8);
                String region = generateRandomString(random, 15);

                byte[] photoBytes = null;
                if (random.nextInt(10) == 0) {
                    photoBytes = new byte[1024 * 1024 * 2];
                } else if (random.nextBoolean()) {
                    photoBytes = new byte[random.nextInt(1000)];
                }
                random.nextBytes(photoBytes != null ? photoBytes : new byte[0]);

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

            MockContentResolver resolver = new MockContentResolver();
            FakeContactProvider provider = new FakeContactProvider(cursor);
            resolver.addProvider(sessionUri.getAuthority(), provider);

            SystemContactsWorkerTask task = new SystemContactsWorkerTask(resolver, sessionUri);
            try {
                task.call();
            } catch (Exception e) {
                throw new AssertionError("Fuzz test failed on iteration " + i, e);
            }
        }
    }

    private static String generateRandomString(java.util.Random random, int maxLength) {
        if (random.nextBoolean()) return null;
        int length = random.nextInt(maxLength);
        StringBuilder sb = new StringBuilder(length);
        for (int i = 0; i < length; i++) {
            sb.append((char) (random.nextInt(96) + 32));
        }
        return sb.toString();
    }
}
