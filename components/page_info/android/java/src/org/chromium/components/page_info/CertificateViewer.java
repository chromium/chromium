// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.page_info;

import org.chromium.base.annotations.NativeMethods;

/**
 * UI component for displaying certificate information.
 */
public class CertificateViewer {

    @NativeMethods
    interface Natives {
        String getCertIssuedToText();
        String getCertInfoCommonNameText();
        String getCertInfoOrganizationText();
        String getCertInfoSerialNumberText();
        String getCertInfoOrganizationUnitText();
        String getCertIssuedByText();
        String getCertValidityText();
        String getCertIssuedOnText();
        String getCertExpiresOnText();
        String getCertFingerprintsText();
        String getCertSHA256FingerprintText();
        String getCertSHA1FingerprintText();
        String getCertExtensionText();
        String getCertSANText();
    }
}
