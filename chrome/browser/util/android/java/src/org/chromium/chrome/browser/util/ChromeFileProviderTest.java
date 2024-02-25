// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.util;

import android.net.Uri;
import android.os.ParcelFileDescriptor;

import androidx.test.filters.LargeTest;
import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.base.test.BaseRobolectricTestRunner;

import java.io.FileNotFoundException;

/**
 * Tests working of ChromeFileProvider.
 *
 * <p>The openFile should be blocked till notify is called. These tests can timeout if the notify
 * does not work correctly.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class ChromeFileProviderTest {
    private ParcelFileDescriptor openFileFromProvider(Uri uri) {
        ChromeFileProvider provider = new ChromeFileProvider();
        ParcelFileDescriptor file = null;
        try {
            provider.openFile(uri, "r");
        } catch (FileNotFoundException e) {
            assert false : "Failed to open file.";
        }
        return file;
    }

    @Test
    @SmallTest
    public void testOpenFileWhenReady() {
        Uri uri = ChromeFileProvider.generateUriAndBlockAccess();
        Uri fileUri = new Uri.Builder().path("1").build();
        ChromeFileProvider.notifyFileReady(uri, fileUri);
        Uri result = ChromeFileProvider.getFileUriWhenReady(uri);
        Assert.assertEquals(result, fileUri);
    }

    @Test
    @LargeTest
    public void testOpenOnAsyncNotify() {
        final Uri uri = ChromeFileProvider.generateUriAndBlockAccess();
        PostTask.postTask(
                TaskTraits.BEST_EFFORT_MAY_BLOCK,
                () -> {
                    try {
                        Thread.sleep(10);
                    } catch (InterruptedException e) {
                        // Ignore exception.
                    }
                    ChromeFileProvider.notifyFileReady(uri, null);
                });
        ParcelFileDescriptor file = openFileFromProvider(uri);
        // File should be null because the notify passes a null file uri.
        Assert.assertNull(file);
    }

    @Test
    @LargeTest
    public void testFileChanged() {
        Uri uri1 = ChromeFileProvider.generateUriAndBlockAccess();
        final Uri uri2 = ChromeFileProvider.generateUriAndBlockAccess();
        final Uri fileUri2 = new Uri.Builder().path("2").build();
        PostTask.postTask(
                TaskTraits.BEST_EFFORT_MAY_BLOCK,
                () -> {
                    try {
                        Thread.sleep(10);
                    } catch (InterruptedException e) {
                        // Ignore exception.
                    }
                    ChromeFileProvider.notifyFileReady(uri2, fileUri2);
                });

        // This should not be blocked even without a notify since file was changed.
        Uri file1 = ChromeFileProvider.getFileUriWhenReady(uri1);
        // File should be null because the notify passes a null file uri.
        Assert.assertNull(file1);

        // This should be unblocked when the notify is called.
        Uri file2 = ChromeFileProvider.getFileUriWhenReady(uri2);
        Assert.assertEquals(fileUri2, file2);

        // This should not be blocked even without a notify since file was changed.
        file1 = ChromeFileProvider.getFileUriWhenReady(uri1);
        // File should be null because the notify passes a null file uri.
        Assert.assertNull(file1);
    }
}
