// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.external_intents;

import android.content.ComponentName;
import android.content.Intent;
import android.text.TextUtils;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.IntentUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

import java.util.Collections;
import java.util.HashSet;
import java.util.Objects;
import java.util.Set;
import java.util.function.Predicate;

/**
 * Evaluates server-configured intent filtering strategies against outgoing external {@link
 * Intent}s.
 *
 * <p>Controlled via two comma-separated Finch parameters under {@link
 * ExternalIntentsFeatures#EXTERNAL_INTENT_FILTER}:
 *
 * <ul>
 *   <li>{@link #PARAM_BLOCK_PACKAGES}: Target package names to block on explicit intents (matched
 *       against {@link Intent#getPackage()} and {@link Intent#getComponent()}). Implicit intents
 *       without an explicit package or component target are not matched by this strategy.
 *   <li>{@link #PARAM_STRIP_EXTRAS}: Specific extra keys to strip in place via {@link
 *       Intent#removeExtra(String)} before allowing the intent to proceed.
 * </ul>
 *
 * <p>Wildcards ({@code *}), whitespace, and malformed entries in server-provided lists are ignored
 * per entry so that a malformed token cannot cause overbroad matching or break valid sibling
 * entries.
 */
@NullMarked
public class ExternalIntentFilter {
    /** Comma-separated list of specific extra keys to remove from outgoing intents. */
    public static final String PARAM_STRIP_EXTRAS = "strip_extras";

    /** Comma-separated list of target package names to block from outgoing intents. */
    public static final String PARAM_BLOCK_PACKAGES = "block_packages";

    /** Strategy for blocking or sanitizing an outgoing external {@link Intent}. */
    @VisibleForTesting
    public interface FilterStrategy {
        /**
         * Evaluates this strategy against {@code intent}.
         *
         * @return {@code true} if the intent should be blocked; {@code false} if navigation may
         *     proceed (after any in-place sanitization).
         */
        boolean shouldBlockOrSanitize(Intent intent);
    }

    /**
     * Strategy that blocks outgoing intents targeting specific package names in {@link
     * #PARAM_BLOCK_PACKAGES}.
     */
    @VisibleForTesting
    public static final class PackageBlockStrategy implements FilterStrategy {
        private final Set<String> mBlockedPackages;

        public PackageBlockStrategy(@Nullable String rawBlockPackages) {
            mBlockedPackages =
                    parseValidTokens(rawBlockPackages, PackageBlockStrategy::isValidPackageName);
        }

        public Set<String> getBlockedPackagesForTesting() {
            return mBlockedPackages;
        }

        @Override
        public boolean shouldBlockOrSanitize(Intent intent) {
            if (mBlockedPackages.isEmpty()) {
                return false;
            }
            String pkg = intent.getPackage();
            if (pkg != null && mBlockedPackages.contains(pkg)) {
                return true;
            }
            ComponentName component = intent.getComponent();
            return component != null && mBlockedPackages.contains(component.getPackageName());
        }

        /**
         * Returns true if {@code pkg} is a specific, non-wildcard Java package name with at least
         * two dot-separated identifier segments.
         */
        @VisibleForTesting
        public static boolean isValidPackageName(String pkg) {
            if (pkg.isEmpty() || pkg.indexOf('*') != -1) {
                return false;
            }
            String[] segments = pkg.split("\\.", -1);
            if (segments.length < 2) {
                return false;
            }
            for (String segment : segments) {
                if (!isValidJavaIdentifier(segment)) {
                    return false;
                }
            }
            return true;
        }

        private static boolean isValidJavaIdentifier(String segment) {
            if (segment.isEmpty()) {
                return false;
            }
            for (int i = 0; i < segment.length(); i++) {
                char c = segment.charAt(i);
                boolean isAlpha = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
                boolean isDigit = (c >= '0' && c <= '9');
                if (i == 0) {
                    if (!isAlpha && c != '_') return false;
                } else {
                    if (!isAlpha && !isDigit && c != '_') return false;
                }
            }
            return true;
        }
    }

    /**
     * Strategy that removes specific extra keys in {@link #PARAM_STRIP_EXTRAS} from outgoing
     * intents in place and allows navigation to proceed.
     */
    @VisibleForTesting
    public static final class ExtraStripStrategy implements FilterStrategy {
        private final Set<String> mStrippedExtras;

        public ExtraStripStrategy(@Nullable String rawStripExtras) {
            mStrippedExtras = parseValidTokens(rawStripExtras, ExtraStripStrategy::isValidExtraKey);
        }

        public Set<String> getStrippedExtrasForTesting() {
            return mStrippedExtras;
        }

        @Override
        public boolean shouldBlockOrSanitize(Intent intent) {
            if (mStrippedExtras.isEmpty()) {
                return false;
            }
            for (String key : mStrippedExtras) {
                if (IntentUtils.safeHasExtra(intent, key)) {
                    intent.removeExtra(key);
                }
            }
            return false;
        }

        /**
         * Returns true if {@code key} is a specific, non-empty extra key containing no wildcards,
         * whitespace, or control characters.
         */
        @VisibleForTesting
        public static boolean isValidExtraKey(String key) {
            if (key.isEmpty() || key.indexOf('*') != -1) {
                return false;
            }
            for (int i = 0; i < key.length(); i++) {
                char c = key.charAt(i);
                if (Character.isWhitespace(c) || c <= 0x1F || c == 0x7F) {
                    return false;
                }
            }
            return true;
        }
    }

    private static Set<String> parseValidTokens(
            @Nullable String rawConfig, Predicate<String> validator) {
        if (TextUtils.isEmpty(rawConfig)) {
            return Collections.emptySet();
        }
        Set<String> validTokens = new HashSet<>();
        for (String token : rawConfig.split(",", -1)) {
            String trimmed = token.trim();
            if (validator.test(trimmed)) {
                validTokens.add(trimmed);
            }
        }
        return Collections.unmodifiableSet(validTokens);
    }

    private static @Nullable String sCachedRawBlockPackages;
    private static @Nullable String sCachedRawStripExtras;
    private static @Nullable PackageBlockStrategy sCachedPackageStrategy;
    private static @Nullable ExtraStripStrategy sCachedExtraStrategy;

    private ExternalIntentFilter() {}

    /**
     * Evaluates the active {@code ExternalIntentFilter} feature strategies against {@code intent}.
     *
     * @param intent The external {@link Intent} about to be resolved or launched.
     * @return {@code true} if the intent is blocked by {@link #PARAM_BLOCK_PACKAGES}; {@code false}
     *     if navigation may proceed (with any keys in {@link #PARAM_STRIP_EXTRAS} removed in
     *     place).
     */
    public static boolean shouldBlockOrSanitize(Intent intent) {
        ThreadUtils.assertOnUiThread();
        if (!ExternalIntentsFeatures.EXTERNAL_INTENT_FILTER.isEnabled()) {
            return false;
        }
        String rawBlockPackages =
                ExternalIntentsFeatures.EXTERNAL_INTENT_FILTER.getFieldTrialParamByFeatureAsString(
                        PARAM_BLOCK_PACKAGES);
        String rawStripExtras =
                ExternalIntentsFeatures.EXTERNAL_INTENT_FILTER.getFieldTrialParamByFeatureAsString(
                        PARAM_STRIP_EXTRAS);
        if (TextUtils.isEmpty(rawBlockPackages) && TextUtils.isEmpty(rawStripExtras)) {
            return false;
        }

        if (sCachedPackageStrategy == null
                || !Objects.equals(rawBlockPackages, sCachedRawBlockPackages)) {
            sCachedRawBlockPackages = rawBlockPackages;
            sCachedPackageStrategy = new PackageBlockStrategy(rawBlockPackages);
        }
        if (sCachedExtraStrategy == null
                || !Objects.equals(rawStripExtras, sCachedRawStripExtras)) {
            sCachedRawStripExtras = rawStripExtras;
            sCachedExtraStrategy = new ExtraStripStrategy(rawStripExtras);
        }

        if (sCachedPackageStrategy.shouldBlockOrSanitize(intent)) {
            return true;
        }
        return sCachedExtraStrategy.shouldBlockOrSanitize(intent);
    }
}
