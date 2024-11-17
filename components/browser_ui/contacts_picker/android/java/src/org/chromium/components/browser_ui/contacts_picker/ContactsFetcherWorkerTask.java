// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.contacts_picker;

import android.content.ContentResolver;
import android.content.Context;
import android.database.Cursor;
import android.net.Uri;
import android.provider.ContactsContract;

import org.chromium.base.ThreadUtils;
import org.chromium.base.task.AsyncTask;
import org.chromium.payments.mojom.PaymentAddress;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

/** A worker task to retrieve images for contacts. */
class ContactsFetcherWorkerTask extends AsyncTask<ArrayList<ContactDetails>> {
    private static final String[] PROJECTION = {
        ContactsContract.Contacts._ID,
        ContactsContract.Contacts.LOOKUP_KEY,
        ContactsContract.Contacts.DISPLAY_NAME_PRIMARY,
    };

    /** An interface to use to communicate back the results to the client. */
    public interface ContactsRetrievedCallback {
        /**
         * A callback to define to receive the contact details.
         *
         * @param contacts The contacts retrieved.
         */
        void contactsRetrieved(ArrayList<ContactDetails> contacts);
    }

    // The content resolver to use for looking up contacts.
    private ContentResolver mContentResolver;

    // The callback to use to communicate the results.
    private ContactsRetrievedCallback mCallback;

    // Whether names were requested by the website.
    private final boolean mIncludeNames;

    // Whether to include emails in the data fetched.
    private final boolean mIncludeEmails;

    // Whether to include telephones in the data fetched.
    private final boolean mIncludeTel;

    // Whether to include addresses in the data fetched.
    private final boolean mIncludeAddresses;

    /**
     * A ContactsFetcherWorkerTask constructor.
     *
     * @param context The Context to use.
     * @param callback The callback to use to communicate back the results.
     * @param includeNames Whether names were requested by the website.
     * @param includeEmails Whether to include emails in the data fetched.
     * @param includeTel Whether to include telephones in the data fetched.
     * @param includeAddresses Whether to include telephones in the data fetched.
     */
    public ContactsFetcherWorkerTask(
            Context context,
            ContactsRetrievedCallback callback,
            boolean includeNames,
            boolean includeEmails,
            boolean includeTel,
            boolean includeAddresses) {
        mContentResolver = context.getContentResolver();
        mCallback = callback;
        mIncludeNames = includeNames;
        mIncludeEmails = includeEmails;
        mIncludeTel = includeTel;
        mIncludeAddresses = includeAddresses;
    }

    /**
     * Fetches the details for all contacts (in a background thread).
     *
     * @return The icon representing a contact.
     */
    @Override
    protected ArrayList<ContactDetails> doInBackground() {
        assert !ThreadUtils.runningOnUiThread();

        if (isCancelled()) return null;

        return getAllContacts();
    }

    /**
     * Fetches specific details for contacts.
     *
     * @param source The source URI to use for the lookup.
     * @param idColumn The name of the id column.
     * @param idColumn The name of the data column.
     * @param sortOrder The sort order. Data must be sorted by CONTACT_ID but can be additionally
     *     sorted also.
     * @return A map of ids to contact details (as ArrayList).
     */
    private Map<String, ArrayList<String>> getDetails(
            Uri source, String idColumn, String dataColumn, String sortOrder) {
        Map<String, ArrayList<String>> map = new HashMap<String, ArrayList<String>>();

        Cursor cursor = mContentResolver.query(source, null, null, null, sortOrder);
        ArrayList<String> list = new ArrayList<String>();
        String key = "";
        String value;
        while (cursor.moveToNext()) {
            String id = cursor.getString(cursor.getColumnIndexOrThrow(idColumn));
            value = cursor.getString(cursor.getColumnIndexOrThrow(dataColumn));
            if (value == null) value = "";
            if (key.isEmpty()) {
                key = id;
                list.add(value);
            } else {
                if (key.equals(id)) {
                    list.add(value);
                } else {
                    map.put(key, list);
                    list = new ArrayList<String>();
                    list.add(value);
                    key = id;
                }
            }
        }
        map.put(key, list);
        cursor.close();

        return map;
    }

    /** Creates a PaymentAddress mojo struct. */
    private PaymentAddress createAddress(
            String city, String country, String formattedAddress, String postcode, String region) {
        PaymentAddress address = new PaymentAddress();

        address.city = city != null ? city : "";
        address.country = country != null ? country : "";
        address.addressLine =
                formattedAddress != null ? new String[] {formattedAddress} : new String[] {};
        address.postalCode = postcode != null ? postcode : "";
        address.region = region != null ? region : "";

        // The other fields are required.
        address.dependentLocality = "";
        address.sortingCode = "";
        address.organization = "";
        address.recipient = "";
        address.phone = "";

        return address;
    }

    /** Fetches all available address info for contacts. */
    private Map<String, ArrayList<PaymentAddress>> getAddressDetails() {
        Map<String, ArrayList<PaymentAddress>> map = new HashMap<>();

        String addressSortOrder =
                ContactsContract.CommonDataKinds.StructuredPostal.CONTACT_ID
                        + " ASC, "
                        + ContactsContract.CommonDataKinds.StructuredPostal.DATA
                        + " ASC";
        Cursor cursor =
                mContentResolver.query(
                        ContactsContract.CommonDataKinds.StructuredPostal.CONTENT_URI,
                        null,
                        null,
                        null,
                        addressSortOrder);

        ArrayList<PaymentAddress> list = new ArrayList<>();
        String key = "";

        while (cursor.moveToNext()) {
            String id =
                    cursor.getString(
                            cursor.getColumnIndexOrThrow(
                                    ContactsContract.CommonDataKinds.StructuredPostal.CONTACT_ID));
            String city =
                    cursor.getString(
                            cursor.getColumnIndexOrThrow(
                                    ContactsContract.CommonDataKinds.StructuredPostal.CITY));
            String country =
                    cursor.getString(
                            cursor.getColumnIndexOrThrow(
                                    ContactsContract.CommonDataKinds.StructuredPostal.COUNTRY));
            String formattedAddress =
                    cursor.getString(
                            cursor.getColumnIndexOrThrow(
                                    ContactsContract.CommonDataKinds.StructuredPostal
                                            .FORMATTED_ADDRESS));
            String postcode =
                    cursor.getString(
                            cursor.getColumnIndexOrThrow(
                                    ContactsContract.CommonDataKinds.StructuredPostal.POSTCODE));
            String region =
                    cursor.getString(
                            cursor.getColumnIndexOrThrow(
                                    ContactsContract.CommonDataKinds.StructuredPostal.REGION));
            PaymentAddress address =
                    createAddress(city, country, formattedAddress, postcode, region);
            if (key.isEmpty()) {
                key = id;
                list.add(address);
            } else {
                if (key.equals(id)) {
                    list.add(address);
                } else {
                    map.put(key, list);
                    list = new ArrayList<>();
                    list.add(address);
                    key = id;
                }
            }
        }
        map.put(key, list);
        cursor.close();

        return map;
    }

    /**
     * Fetches all known contacts.
     *
     * @return The contact list as an array.
     */
    public ArrayList<ContactDetails> getAllContacts() {
        Map<String, ArrayList<String>> emailMap =
                mIncludeEmails
                        ? getDetails(
                                ContactsContract.CommonDataKinds.Email.CONTENT_URI,
                                ContactsContract.CommonDataKinds.Email.CONTACT_ID,
                                ContactsContract.CommonDataKinds.Email.DATA,
                                ContactsContract.CommonDataKinds.Email.CONTACT_ID
                                        + " ASC, "
                                        + ContactsContract.CommonDataKinds.Email.DATA
                                        + " ASC")
                        : null;

        Map<String, ArrayList<String>> phoneMap =
                mIncludeTel
                        ? getDetails(
                                ContactsContract.CommonDataKinds.Phone.CONTENT_URI,
                                ContactsContract.CommonDataKinds.Phone.CONTACT_ID,
                                ContactsContract.CommonDataKinds.Phone.DATA,
                                ContactsContract.CommonDataKinds.Phone.CONTACT_ID
                                        + " ASC, "
                                        + ContactsContract.CommonDataKinds.Phone.NUMBER
                                        + " ASC")
                        : null;

        Map<String, ArrayList<PaymentAddress>> addressMap =
                mIncludeAddresses ? getAddressDetails() : null;

        // A cursor containing the raw contacts data.
        Cursor cursor =
                mContentResolver.query(
                        ContactsContract.Contacts.CONTENT_URI,
                        PROJECTION,
                        null,
                        null,
                        ContactsContract.Contacts.SORT_KEY_PRIMARY + " ASC");
        if (!cursor.moveToFirst()) {
            cursor.close();
            return new ArrayList<ContactDetails>();
        }

        ArrayList<ContactDetails> contacts = new ArrayList<ContactDetails>(cursor.getCount());
        do {
            String id =
                    cursor.getString(cursor.getColumnIndexOrThrow(ContactsContract.Contacts._ID));
            String name =
                    cursor.getString(
                            cursor.getColumnIndexOrThrow(
                                    ContactsContract.Contacts.DISPLAY_NAME_PRIMARY));
            List<String> email = mIncludeEmails ? emailMap.get(id) : null;
            List<String> tel = mIncludeTel ? phoneMap.get(id) : null;
            List<PaymentAddress> address = mIncludeAddresses ? addressMap.get(id) : null;

            if (mIncludeNames || email != null || tel != null || address != null) {
                contacts.add(new ContactDetails(id, name, email, tel, address));
            }
        } while (cursor.moveToNext());

        cursor.close();
        return contacts;
    }

    /**
     * Communicates the results back to the client. Called on the UI thread.
     *
     * @param contacts The contacts retrieved.
     */
    @Override
    protected void onPostExecute(ArrayList<ContactDetails> contacts) {
        assert ThreadUtils.runningOnUiThread();

        if (isCancelled()) return;

        mCallback.contactsRetrieved(contacts);
    }
}
