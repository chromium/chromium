// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.payments;

/** Java equivalent of components/payments/content/web_app_manifest.h:WebAppManifestSection */
public final class WebAppManifestSection {
    /**
     * Constructor that does not set the fingerprints. They have to be set after the object is
     * created.
     * @param id The package name of the app.
     * @param minVersion The minimum version number of the app.
     * @param numberOfFingerprints The size of the fingerprints array that are expected to be set
     * once the object has been constructed.
     */
    public WebAppManifestSection(String id, long minVersion, int numberOfFingerprints) {
        this.id = id;
        this.minVersion = minVersion;
        this.fingerprints = new byte[numberOfFingerprints][];
    }

    /**
     * Constructor that sets the fingerprints.
     * @param id The package name of the app.
     * @param minVersion The minimum version number of the app.
     * @param fingerprints The result of SHA256 (signing certificate bytes) for each certificate in
     * the app.
     */
    public WebAppManifestSection(String id, long minVersion, byte[][] fingerprints) {
        this.id = id;
        this.minVersion = minVersion;
        this.fingerprints = fingerprints;
    }

    /** The {@link String} representing the package name of the app. */
    public final String id;

    /** The minimum version number of the app. */
    public final long minVersion;

    /** The result of SHA256 (signing certificate bytes) for each certificate in the app. */
    public final byte[][] fingerprints;
}
