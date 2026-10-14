// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.sync_device_info;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.JniType;

import org.chromium.build.annotations.NullMarked;

/**
 * Mirrors the native struct {@code syncer::RemoteDeviceInfo} declared in
 * //components/sync_device_info/remote_device_info.h.
 */
@JNINamespace("syncer")
@NullMarked
public class RemoteDeviceInfo {
    public final String deviceName;
    public final String cacheGuid;
    public final @FormFactor int formFactor;
    public final @OsType int osType;
    public final String lastActiveTimeForDisplay;

    @CalledByNative
    public RemoteDeviceInfo(
            @JniType("std::string") String deviceName,
            @JniType("std::string") String cacheGuid,
            @JniType("syncer::DeviceInfo::FormFactor") @FormFactor int formFactor,
            @JniType("syncer::DeviceInfo::OsType") @OsType int osType,
            @JniType("std::u16string") String lastActiveTimeForDisplay) {
        this.deviceName = deviceName;
        this.cacheGuid = cacheGuid;
        this.formFactor = formFactor;
        this.osType = osType;
        this.lastActiveTimeForDisplay = lastActiveTimeForDisplay;
    }
}
