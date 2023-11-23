// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.content_relationship_verification;

import org.chromium.components.embedder_support.util.Origin;

import java.util.Collections;
import java.util.List;

/**
 * This is a plain-old-data class to store a Digital Asset Link relationship [1].
 *
 * [1] https://developers.google.com/digital-asset-links/v1/getting-started
 */
public class Relationship {
    public final String packageName;
    public final Origin origin;
    public final String relation;
    public final List<String> signatureFingerprints;

    /** Creates a {@link Relationship} to hold relationship details. */
    public Relationship(
            String packageName,
            List<String> signatureFingerprints,
            Origin origin,
            String relation) {
        this.packageName = packageName;
        this.signatureFingerprints = signatureFingerprints;
        this.origin = origin;
        this.relation = relation;
    }

    /**
     * Serializes the Relationship to a String. This is used when storing relationships in
     * AndroidPreferences, so needs to be stable.
     */
    @Override
    public String toString() {
        // Neither package names nor origins contain commas.
        String fingerprints = "";
        if (signatureFingerprints != null) {
            Collections.sort(signatureFingerprints);
            fingerprints = String.join(",", signatureFingerprints);
        }
        return packageName + "," + origin + "," + relation + "," + fingerprints;
    }
}
