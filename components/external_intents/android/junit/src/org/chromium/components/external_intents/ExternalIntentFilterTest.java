// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.external_intents;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import android.content.ComponentName;
import android.content.Intent;
import android.net.Uri;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.FeatureOverrides;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.components.external_intents.ExternalIntentFilter.ExtraStripStrategy;
import org.chromium.components.external_intents.ExternalIntentFilter.PackageBlockStrategy;

import java.util.Set;

/**
 * Unit tests for {@link ExternalIntentFilter}, covering {@link PackageBlockStrategy}, {@link
 * ExtraStripStrategy}, and rejection of wildcards or malformed server-provided entries.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class ExternalIntentFilterTest {

    @Test
    public void testExtraStripStrategy_RemovesMatchingKeysAndKeepsOthers() {
        ExtraStripStrategy strategy =
                new ExtraStripStrategy("CUSTOM_PLUGIN_PATH, SECOND_DANGEROUS_KEY");

        Intent intent = new Intent(Intent.ACTION_VIEW, Uri.parse("https://example.com/doc.pdf"));
        intent.putExtra("CUSTOM_PLUGIN_PATH", "/sdcard/Download/payload.jar");
        intent.putExtra("SECOND_DANGEROUS_KEY", true);
        intent.putExtra("SAFE_DOCUMENT_TITLE", "Quarterly Report");

        assertFalse(strategy.shouldBlockOrSanitize(intent));
        assertFalse(intent.hasExtra("CUSTOM_PLUGIN_PATH"));
        assertFalse(intent.hasExtra("SECOND_DANGEROUS_KEY"));
        assertEquals("Quarterly Report", intent.getStringExtra("SAFE_DOCUMENT_TITLE"));
    }

    @Test
    public void testPackageBlockStrategy_BlocksMatchingIntentPackageOrComponent() {
        PackageBlockStrategy strategy =
                new PackageBlockStrategy("com.example.vulnerable, org.example.other");

        // Matches via Intent.getPackage().
        Intent packageIntent = new Intent(Intent.ACTION_VIEW);
        packageIntent.setPackage("com.example.vulnerable");
        assertTrue(strategy.shouldBlockOrSanitize(packageIntent));

        // Matches via Intent.getComponent().getPackageName() when Intent.getPackage() is unset
        // (prior to sanitizeQueryIntentActivitiesIntent clearing the component).
        Intent componentIntent = new Intent(Intent.ACTION_VIEW);
        componentIntent.setComponent(
                new ComponentName("org.example.other", "org.example.other.MainActivity"));
        assertTrue(strategy.shouldBlockOrSanitize(componentIntent));

        // Unlisted package is allowed.
        Intent safeIntent = new Intent(Intent.ACTION_VIEW);
        safeIntent.setPackage("com.example.safe");
        assertFalse(strategy.shouldBlockOrSanitize(safeIntent));

        // Intent with neither package nor component is allowed.
        Intent implicitIntent = new Intent(Intent.ACTION_VIEW, Uri.parse("https://example.com"));
        assertFalse(strategy.shouldBlockOrSanitize(implicitIntent));
    }

    @Test
    public void testPackageBlockStrategy_RejectsWildcardsAndMalformedPackages() {
        PackageBlockStrategy strategy =
                new PackageBlockStrategy(
                        "*, .*, com.*, com.example.*, *example.app, singleword, .leading.dot,"
                                + " trailing.dot., com..double, com.bad-dash.app,"
                                + " com.bad space.app, com.1digitstart.app, , com.valid.package_1");

        // Only the valid multi-segment package name survives; all wildcards and malformed entries
        // are ignored.
        assertEquals(Set.of("com.valid.package_1"), strategy.getBlockedPackagesForTesting());

        Intent unblockedIntent = new Intent(Intent.ACTION_VIEW);
        unblockedIntent.setPackage("com.example.anything");
        assertFalse(strategy.shouldBlockOrSanitize(unblockedIntent));

        Intent blockedIntent = new Intent(Intent.ACTION_VIEW);
        blockedIntent.setPackage("com.valid.package_1");
        assertTrue(strategy.shouldBlockOrSanitize(blockedIntent));
    }

    @Test
    public void testExtraStripStrategy_RejectsWildcardsAndWhitespace() {
        ExtraStripStrategy strategy =
                new ExtraStripStrategy(
                        "*, FOO*, *BAR, MID*STAR, KEY WITH SPACE, KEY\tTAB, KEY\nNEWLINE, ,"
                                + " VALID_EXTRA_KEY, com.example.extra.KEY_2");

        // Only exact, whitespace-free, wildcard-free keys survive.
        assertEquals(
                Set.of("VALID_EXTRA_KEY", "com.example.extra.KEY_2"),
                strategy.getStrippedExtrasForTesting());

        Intent intent = new Intent(Intent.ACTION_VIEW);
        intent.putExtra("FOO_SOMETHING", "keep");
        intent.putExtra("SOMETHING_BAR", "keep");
        intent.putExtra("VALID_EXTRA_KEY", "strip");
        intent.putExtra("com.example.extra.KEY_2", "strip");

        assertFalse(strategy.shouldBlockOrSanitize(intent));
        assertEquals("keep", intent.getStringExtra("FOO_SOMETHING"));
        assertEquals("keep", intent.getStringExtra("SOMETHING_BAR"));
        assertFalse(intent.hasExtra("VALID_EXTRA_KEY"));
        assertFalse(intent.hasExtra("com.example.extra.KEY_2"));
    }

    @Test
    public void testFeatureIntegration_ShouldBlockOrSanitize() {
        FeatureOverrides.newBuilder()
                .enable(ExternalIntentsFeatures.EXTERNAL_INTENT_FILTER_NAME)
                .param(ExternalIntentFilter.PARAM_BLOCK_PACKAGES, "com.example.blocked.app")
                .param(ExternalIntentFilter.PARAM_STRIP_EXTRAS, "DANGEROUS_EXTRA_KEY")
                .apply();

        // Blocked package returns true immediately.
        Intent blockedIntent = new Intent(Intent.ACTION_VIEW, Uri.parse("https://example.com"));
        blockedIntent.setPackage("com.example.blocked.app");
        assertTrue(ExternalIntentFilter.shouldBlockOrSanitize(blockedIntent));

        // Allowed package has matching extra stripped and returns false.
        Intent sanitizedIntent = new Intent(Intent.ACTION_VIEW, Uri.parse("https://example.com"));
        sanitizedIntent.setPackage("com.example.allowed.app");
        sanitizedIntent.putExtra("DANGEROUS_EXTRA_KEY", "/sdcard/payload.dex");
        sanitizedIntent.putExtra("BENIGN_EXTRA_KEY", "ok");

        assertFalse(ExternalIntentFilter.shouldBlockOrSanitize(sanitizedIntent));
        assertFalse(sanitizedIntent.hasExtra("DANGEROUS_EXTRA_KEY"));
        assertEquals("ok", sanitizedIntent.getStringExtra("BENIGN_EXTRA_KEY"));
    }

    @Test
    public void testFeatureDisabled_NoOp() {
        FeatureOverrides.newBuilder()
                .disable(ExternalIntentsFeatures.EXTERNAL_INTENT_FILTER_NAME)
                .param(
                        ExternalIntentsFeatures.EXTERNAL_INTENT_FILTER_NAME,
                        ExternalIntentFilter.PARAM_BLOCK_PACKAGES,
                        "com.example.blocked.app")
                .param(
                        ExternalIntentsFeatures.EXTERNAL_INTENT_FILTER_NAME,
                        ExternalIntentFilter.PARAM_STRIP_EXTRAS,
                        "DANGEROUS_EXTRA_KEY")
                .apply();

        Intent intent = new Intent(Intent.ACTION_VIEW);
        intent.setPackage("com.example.blocked.app");
        intent.putExtra("DANGEROUS_EXTRA_KEY", "value");

        assertFalse(ExternalIntentFilter.shouldBlockOrSanitize(intent));
        assertEquals("value", intent.getStringExtra("DANGEROUS_EXTRA_KEY"));
    }

    @Test
    public void testFeatureEnabledWithUnsetOrSingleParam_DoesNotCallNative() {
        // Both params unset: must fall back to "" without calling into native JNI.
        FeatureOverrides.enable(ExternalIntentsFeatures.EXTERNAL_INTENT_FILTER_NAME);
        Intent intent = new Intent(Intent.ACTION_VIEW, Uri.parse("https://example.com"));
        intent.setPackage("com.example.app");
        assertFalse(ExternalIntentFilter.shouldBlockOrSanitize(intent));

        // Only one param overridden: the unset sibling param must not call into native JNI.
        FeatureOverrides.newBuilder()
                .enable(ExternalIntentsFeatures.EXTERNAL_INTENT_FILTER_NAME)
                .param(ExternalIntentFilter.PARAM_BLOCK_PACKAGES, "com.example.blocked.app")
                .apply();
        Intent blockedIntent = new Intent(Intent.ACTION_VIEW, Uri.parse("https://example.com"));
        blockedIntent.setPackage("com.example.blocked.app");
        assertTrue(ExternalIntentFilter.shouldBlockOrSanitize(blockedIntent));
    }
}
