// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.background_task_scheduler.internal;

import android.os.Build;
import android.os.Bundle;
import android.os.PersistableBundle;

import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.MinAndroidSdkLevel;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.Set;

/** Tests for {@link BundleToPersistableBundleConverter}. */
@RunWith(BaseRobolectricTestRunner.class)
@MinAndroidSdkLevel(Build.VERSION_CODES.LOLLIPOP_MR1)
public class BundleToPersistableBundleConverterTest {
    @Test
    public void testAllValidConversions() {
        Bundle bundle = new Bundle();
        bundle.putString("s", "bar");
        bundle.putStringArray("sa", new String[] {"b", "a", "r"});
        bundle.putBoolean("b", true);
        bundle.putBooleanArray("ba", new boolean[] {true, false, true});
        bundle.putInt("i", 1342543);
        bundle.putIntArray("ia", new int[] {1, 2, 3});
        bundle.putLong("l", 1342543L);
        bundle.putLongArray("la", new long[] {1L, 2L, 3L});
        bundle.putDouble("d", 5.3223);
        bundle.putDoubleArray("da", new double[] {5.3223, 42.42});

        BundleToPersistableBundleConverter.Result result =
                BundleToPersistableBundleConverter.convert(bundle);
        PersistableBundle pBundle = result.getPersistableBundle();

        Assert.assertFalse(result.hasErrors());
        Assert.assertEquals(bundle.getString("s"), pBundle.getString("s"));
        Assert.assertTrue(Arrays.equals(bundle.getStringArray("sa"), pBundle.getStringArray("sa")));
        Assert.assertEquals(bundle.getBoolean("b"), pBundle.getBoolean("b"));
        Assert.assertTrue(
                Arrays.equals(bundle.getBooleanArray("ba"), pBundle.getBooleanArray("ba")));
        Assert.assertEquals(bundle.getInt("i"), pBundle.getInt("i"));
        Assert.assertTrue(Arrays.equals(bundle.getIntArray("ia"), pBundle.getIntArray("ia")));
        Assert.assertEquals(bundle.getLong("l"), pBundle.getLong("l"));
        Assert.assertTrue(Arrays.equals(bundle.getLongArray("la"), pBundle.getLongArray("la")));
        Assert.assertEquals(bundle.getDouble("d"), pBundle.getDouble("d"), 0);
        Assert.assertTrue(Arrays.equals(bundle.getDoubleArray("da"), pBundle.getDoubleArray("da")));
    }

    @Test
    public void testSomeBadConversions() {
        Bundle bundle = new Bundle();
        bundle.putString("s", "this should be there");
        bundle.putByte("byte", (byte) 0x30);
        bundle.putFloat("float", 14.04F);
        ArrayList<String> arrayList = new ArrayList<>();
        arrayList.add("a");
        arrayList.add("b");
        bundle.putStringArrayList("arrayList", arrayList);

        BundleToPersistableBundleConverter.Result result =
                BundleToPersistableBundleConverter.convert(bundle);

        Assert.assertTrue(result.hasErrors());
        Set<String> failedKeys = result.getFailedKeys();
        Assert.assertEquals(3, failedKeys.size());
        Assert.assertTrue(failedKeys.contains("byte"));
        Assert.assertTrue(failedKeys.contains("float"));
        Assert.assertTrue(failedKeys.contains("arrayList"));
        Assert.assertEquals(bundle.getString("s"), result.getPersistableBundle().getString("s"));
    }

    @Test
    public void testNullValue() {
        Bundle bundle = new Bundle();
        bundle.putString("foo", "value1");
        bundle.putString("bar", "");
        bundle.putString("qux", null);

        BundleToPersistableBundleConverter.Result result =
                BundleToPersistableBundleConverter.convert(bundle);

        Assert.assertFalse(result.hasErrors());
        Assert.assertEquals(
                bundle.getString("foo"), result.getPersistableBundle().getString("foo"));
        Assert.assertEquals(
                bundle.getString("bar"), result.getPersistableBundle().getString("bar"));
        Assert.assertEquals(
                bundle.getString("qux"), result.getPersistableBundle().getString("qux"));
    }
}
