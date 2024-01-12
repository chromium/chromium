// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.content_settings;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.content_public.browser.BrowserContextHandle;

/**
 * Observer class that receives updates for Content settings. The instance will be attached to
 * content settings provider when it is created.
 */
@JNINamespace("content_settings")
public abstract class ContentSettingsObserver {
    private final long mNativeAndroidObserver;
    private boolean mIsDestroyed;

    /**
     * Create an observer for content settings changes and start monitoring.
     * @param contextHandle The native BrowserContext.
     */
    public ContentSettingsObserver(BrowserContextHandle contextHandle) {
        mNativeAndroidObserver = ContentSettingsObserverJni.get().init(this, contextHandle);
    }

    @CalledByNative
    private void onContentSettingChanged(
            String primaryPattern,
            String secondaryPattern,
            @ContentSettingsType.EnumType int contentSettingsType) {
        onContentSettingChanged(
                primaryPattern, secondaryPattern, new ContentSettingsTypeSet(contentSettingsType));
    }

    /**
     * Abstract function that will be invoked when content settings is changed.
     * @param primaryPattern The primary pattern for the changed content settings.
     * @param secondaryPattern The secondary pattern for the changed content settings.
     * @param contentSettingsTypeSet The {@link ContentSettingsTypeSet} that is being changed.
     */
    protected abstract void onContentSettingChanged(
            String primaryPattern,
            String secondaryPattern,
            ContentSettingsTypeSet contentSettingsTypeSet);

    /** Destroy the linked native object and stop listen to content settings changes. */
    public void destroy() {
        assert !mIsDestroyed : "This observer is already destroyed.";
        mIsDestroyed = true;
        ContentSettingsObserverJni.get().destroy(mNativeAndroidObserver, this);
    }

    @NativeMethods
    interface Natives {
        long init(ContentSettingsObserver caller, BrowserContextHandle contextHandle);

        void destroy(long nativeAndroidObserver, ContentSettingsObserver caller);
    }
}
