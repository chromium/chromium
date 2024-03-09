// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.privacy_sandbox;

/** Interface implemented by the embedder to access IpProtection embedder-specific logic . */
public interface IpProtectionDelegate {
    /**
     * @return whether Ip protection is enabled.
     */
    boolean isIpProtectionEnabled();

    /** Set the value of Ip protection pref. */
    void setIpProtection(boolean enabled);
}
