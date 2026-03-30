// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net.impl;

/**
 * Sentinel class for enabling/disabling NativeCronetProvider. See {@link
 * NativeCronetProvider#isEnabled()}.
 *
 * <p>This class must be present in the final build for NativeCronetProvider to be enabled.
 *
 * <p>This makes it possible to disable NativeCronetProvider by making sure this class does not make
 * it into the build. This is useful to support some packaging schemes where the Java code is
 * shipped separately from the native code - this class can be shipped alongside the native code,
 * such that the provider automatically disables itself if the native library is not present.
 * (Omitting this sentinel class is arguably equivalent to letting R8/ProGuard remove
 * NativeCronetProvider altogether, but this approach has the benefit of producing predictable
 * runtime behavior even if R8/ProGuard is not used.)
 */
final class NativeCronetProviderSentinel {
    private NativeCronetProviderSentinel() {}
}
