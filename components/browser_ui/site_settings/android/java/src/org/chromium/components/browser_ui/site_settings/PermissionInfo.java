// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.site_settings;

import androidx.annotation.Nullable;

import org.chromium.components.content_settings.ContentSettingValues;
import org.chromium.components.content_settings.ContentSettingsType;
import org.chromium.components.embedder_support.browser_context.BrowserContextHandle;

import java.io.Serializable;

/**
 * Permission information for a given origin.
 */
public class PermissionInfo implements Serializable {
    private final boolean mIsIncognito;
    private final boolean mIsEmbargoed;
    private final String mEmbedder;
    private final String mOrigin;
    private final @ContentSettingsType int mContentSettingsType;

    public PermissionInfo(
            @ContentSettingsType int type, String origin, String embedder, boolean isIncognito) {
        this(type, origin, embedder, isIncognito, false);
    }

    public PermissionInfo(@ContentSettingsType int type, String origin, String embedder,
            boolean isIncognito, boolean isEmbargoed) {
        mOrigin = origin;
        mEmbedder = embedder;
        mIsIncognito = isIncognito;
        mContentSettingsType = type;
        mIsEmbargoed = isEmbargoed;
    }

    public @ContentSettingsType int getContentSettingsType() {
        return mContentSettingsType;
    }

    public String getOrigin() {
        return mOrigin;
    }

    public String getEmbedder() {
        return mEmbedder;
    }

    public boolean isIncognito() {
        return mIsIncognito;
    }

    public String getEmbedderSafe() {
        return mEmbedder != null ? mEmbedder : mOrigin;
    }

    public boolean isEmbargoed() {
        return mIsEmbargoed;
    }

    /**
     * Returns the ContentSetting value for this origin.
     */
    public @ContentSettingValues @Nullable Integer getContentSetting(
            BrowserContextHandle browserContextHandle) {
        switch (mContentSettingsType) {
            case ContentSettingsType.AR:
                return WebsitePreferenceBridgeJni.get().getArSettingForOrigin(
                        browserContextHandle, mOrigin, getEmbedderSafe());
            case ContentSettingsType.MEDIASTREAM_CAMERA:
                return WebsitePreferenceBridgeJni.get().getCameraSettingForOrigin(
                        browserContextHandle, mOrigin, getEmbedderSafe());
            case ContentSettingsType.CLIPBOARD_READ_WRITE:
                return WebsitePreferenceBridgeJni.get().getClipboardSettingForOrigin(
                        browserContextHandle, mOrigin);
            case ContentSettingsType.GEOLOCATION:
                return WebsitePreferenceBridgeJni.get().getGeolocationSettingForOrigin(
                        browserContextHandle, mOrigin, getEmbedderSafe());
            case ContentSettingsType.IDLE_DETECTION:
                return WebsitePreferenceBridgeJni.get().getIdleDetectionSettingForOrigin(
                        browserContextHandle, mOrigin, getEmbedderSafe());
            case ContentSettingsType.MEDIASTREAM_MIC:
                return WebsitePreferenceBridgeJni.get().getMicrophoneSettingForOrigin(
                        browserContextHandle, mOrigin, getEmbedderSafe());
            case ContentSettingsType.MIDI_SYSEX:
                return WebsitePreferenceBridgeJni.get().getMidiSettingForOrigin(
                        browserContextHandle, mOrigin, getEmbedderSafe());
            case ContentSettingsType.NFC:
                return WebsitePreferenceBridgeJni.get().getNfcSettingForOrigin(
                        browserContextHandle, mOrigin, getEmbedderSafe());
            case ContentSettingsType.NOTIFICATIONS:
                return WebsitePreferenceBridgeJni.get().getNotificationSettingForOrigin(
                        browserContextHandle, mOrigin);
            case ContentSettingsType.PROTECTED_MEDIA_IDENTIFIER:
                return WebsitePreferenceBridgeJni.get().getProtectedMediaIdentifierSettingForOrigin(
                        browserContextHandle, mOrigin, getEmbedderSafe());
            case ContentSettingsType.SENSORS:
                return WebsitePreferenceBridgeJni.get().getSensorsSettingForOrigin(
                        browserContextHandle, mOrigin, getEmbedderSafe());
            case ContentSettingsType.VR:
                return WebsitePreferenceBridgeJni.get().getVrSettingForOrigin(
                        browserContextHandle, mOrigin, getEmbedderSafe());
            default:
                assert false;
                return null;
        }
    }

    /**
     * Sets the native ContentSetting value for this origin.
     */
    public void setContentSetting(
            BrowserContextHandle browserContextHandle, @ContentSettingValues int value) {
        switch (mContentSettingsType) {
            case ContentSettingsType.AR:
                WebsitePreferenceBridgeJni.get().setArSettingForOrigin(
                        browserContextHandle, mOrigin, getEmbedderSafe(), value);
                break;
            case ContentSettingsType.MEDIASTREAM_CAMERA:
                WebsitePreferenceBridgeJni.get().setCameraSettingForOrigin(
                        browserContextHandle, mOrigin, value);
                break;
            case ContentSettingsType.CLIPBOARD_READ_WRITE:
                WebsitePreferenceBridgeJni.get().setClipboardSettingForOrigin(
                        browserContextHandle, mOrigin, value);
                break;
            case ContentSettingsType.GEOLOCATION:
                WebsitePreferenceBridgeJni.get().setGeolocationSettingForOrigin(
                        browserContextHandle, mOrigin, getEmbedderSafe(), value);
                break;
            case ContentSettingsType.IDLE_DETECTION:
                WebsitePreferenceBridgeJni.get().setIdleDetectionSettingForOrigin(
                        browserContextHandle, mOrigin, getEmbedderSafe(), value);
                break;
            case ContentSettingsType.MEDIASTREAM_MIC:
                WebsitePreferenceBridgeJni.get().setMicrophoneSettingForOrigin(
                        browserContextHandle, mOrigin, value);
                break;
            case ContentSettingsType.MIDI_SYSEX:
                WebsitePreferenceBridgeJni.get().setMidiSettingForOrigin(
                        browserContextHandle, mOrigin, getEmbedderSafe(), value);
                break;
            case ContentSettingsType.NFC:
                WebsitePreferenceBridgeJni.get().setNfcSettingForOrigin(
                        browserContextHandle, mOrigin, getEmbedderSafe(), value);
                break;
            case ContentSettingsType.NOTIFICATIONS:
                WebsitePreferenceBridgeJni.get().setNotificationSettingForOrigin(
                        browserContextHandle, mOrigin, value);
                break;
            case ContentSettingsType.PROTECTED_MEDIA_IDENTIFIER:
                WebsitePreferenceBridgeJni.get().setProtectedMediaIdentifierSettingForOrigin(
                        browserContextHandle, mOrigin, getEmbedderSafe(), value);
                break;
            case ContentSettingsType.SENSORS:
                WebsitePreferenceBridgeJni.get().setSensorsSettingForOrigin(
                        browserContextHandle, mOrigin, getEmbedderSafe(), value);
                break;
            case ContentSettingsType.VR:
                WebsitePreferenceBridgeJni.get().setVrSettingForOrigin(
                        browserContextHandle, mOrigin, getEmbedderSafe(), value);
                break;
            default:
                assert false;
        }
    }
}
