// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.content_relationship_verification;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.ThreadUtils;
import org.chromium.components.embedder_support.util.Origin;

import java.util.Arrays;
import java.util.Collections;
import java.util.HashSet;
import java.util.Set;

/**
 * Stores the results of Digital Asset Link verifications performed by {@link OriginVerifier}.
 *
 * There are two types of results stored - proper relationships that are stored in the Child-classes
 * and overrides that are stored in a static variable (therefore not persisted across
 * re-launches). Ideally, we will be able to get rid of the overrides in the future, they're just
 * here now for legacy reasons.
 *
 * Lifecycle: This class is a singleton, however you should constructor inject the singleton
 * instance to your classes where possible to make testing easier.
 * Thread safety: Methods can be called on any thread.
 */
public abstract class VerificationResultStore {
    /**
     * A collection of Relationships (stored as Strings, with the signature set to an empty String)
     * that we override verifications to succeed for. It is threadsafe.
     */
    private static final Set<String> sVerificationOverrides =
            Collections.synchronizedSet(new HashSet<>());

    public void addRelationship(Relationship relationship) {
        Set<String> savedLinks = getRelationships();
        savedLinks.add(relationship.toString());
        setRelationships(savedLinks);
    }

    public void removeRelationship(Relationship relationship) {
        Set<String> savedLinks = getRelationships();
        savedLinks.remove(relationship.toString());
        setRelationships(savedLinks);
    }

    public boolean isRelationshipSaved(Relationship relationship) {
        return getRelationships().contains(relationship.toString());
    }

    public void clearStoredRelationships() {
        ThreadUtils.assertOnUiThread();
        setRelationships(Collections.emptySet());
        sVerificationOverrides.clear();
    }

    public void addOverride(String packageName, Origin origin, String relationship) {
        sVerificationOverrides.add(overrideToString(packageName, origin, relationship));
    }

    public boolean shouldOverride(String packageName, Origin origin, String relationship) {
        return sVerificationOverrides.contains(overrideToString(packageName, origin, relationship));
    }

    private static String overrideToString(String packageName, Origin origin, String relationship) {
        return new Relationship(packageName, Arrays.asList(""), origin, relationship).toString();
    }

    @VisibleForTesting
    protected abstract Set<String> getRelationships();

    @VisibleForTesting
    protected abstract void setRelationships(Set<String> relationships);
}
