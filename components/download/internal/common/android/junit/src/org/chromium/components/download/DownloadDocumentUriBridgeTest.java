// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.download;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import android.content.ComponentName;
import android.content.Context;
import android.content.IntentFilter;
import android.content.pm.ProviderInfo;
import android.net.Uri;
import android.provider.DocumentsContract;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.Shadows;
import org.robolectric.shadows.ShadowPackageManager;

import org.chromium.base.ContextUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;

/** Unit tests for {@link DownloadDocumentUriBridge}. */
@RunWith(BaseRobolectricTestRunner.class)
public class DownloadDocumentUriBridgeTest {
    private static final String DOCUMENTS_AUTHORITY = "org.chromium.test.documents";

    @Before
    public void setUp() {
        Context context = ContextUtils.getApplicationContext();
        ShadowPackageManager shadowPackageManager = Shadows.shadowOf(context.getPackageManager());

        // Register a DocumentsProvider to handle DOCUMENTS_AUTHORITY.
        ProviderInfo providerInfo = new ProviderInfo();
        providerInfo.packageName = context.getPackageName();
        providerInfo.name = "org.chromium.test.documents.TestDocumentsProvider";
        providerInfo.authority = DOCUMENTS_AUTHORITY;
        shadowPackageManager.addOrUpdateProvider(providerInfo);
        shadowPackageManager.addIntentFilterForProvider(
                new ComponentName(providerInfo.packageName, providerInfo.name),
                new IntentFilter(DocumentsContract.PROVIDER_INTERFACE));
    }

    @Test
    public void testIsDocumentUri_NullAndEmpty() {
        assertFalse(DownloadDocumentUriBridge.isDocumentUri(null));
        assertFalse(DownloadDocumentUriBridge.isDocumentUri(""));
        assertFalse(DownloadDocumentUriBridge.isDocumentUri("   "));
    }

    @Test
    public void testIsDocumentUri_Malformed() {
        assertFalse(DownloadDocumentUriBridge.isDocumentUri("://malformed:uri"));
        assertFalse(DownloadDocumentUriBridge.isDocumentUri("content://"));
        assertFalse(DownloadDocumentUriBridge.isDocumentUri("invalid_uri_string"));
    }

    @Test
    public void testIsDocumentUri_NonContentUri() {
        assertFalse(
                DownloadDocumentUriBridge.isDocumentUri(
                        "file:///storage/emulated/0/Download/test.pdf"));
        assertFalse(
                DownloadDocumentUriBridge.isDocumentUri("https://example.com/download/test.pdf"));
        assertFalse(DownloadDocumentUriBridge.isDocumentUri("http://example.com/test.bin"));
    }

    @Test
    public void testIsDocumentUri_MediaStoreUri() {
        assertFalse(
                DownloadDocumentUriBridge.isDocumentUri(
                        "content://media/external/downloads/12345"));
        assertFalse(
                DownloadDocumentUriBridge.isDocumentUri(
                        "content://media/external/images/media/6789"));
    }

    @Test
    public void testIsDocumentUri_SafDocumentUri() {
        Uri docUri = DocumentsContract.buildDocumentUri(DOCUMENTS_AUTHORITY, "document:123");
        assertTrue(DownloadDocumentUriBridge.isDocumentUri(docUri.toString()));
    }

    @Test
    public void testRenameDocumentUri_EmptyInputs() {
        assertFalse(DownloadDocumentUriBridge.renameDocumentUri(null, "new_name.pdf"));
        assertFalse(DownloadDocumentUriBridge.renameDocumentUri("", "new_name.pdf"));
        assertFalse(DownloadDocumentUriBridge.renameDocumentUri("content://doc", null));
        assertFalse(DownloadDocumentUriBridge.renameDocumentUri("content://doc", ""));
    }

    @Test
    public void testRenameDocumentUri_InvalidUri() {
        assertFalse(DownloadDocumentUriBridge.renameDocumentUri("://invalid", "new_name.pdf"));
    }
}
