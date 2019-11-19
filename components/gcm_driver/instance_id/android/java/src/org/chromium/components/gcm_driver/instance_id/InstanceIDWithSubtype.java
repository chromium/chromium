// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.gcm_driver.instance_id;

import android.content.Context;
import android.os.Bundle;
import android.text.TextUtils;

import androidx.annotation.VisibleForTesting;

import com.google.android.gms.iid.InstanceID;

import org.chromium.base.ContextUtils;

import java.io.IOException;
import java.util.HashMap;
import java.util.Map;

/**
 * InstanceID wrapper that allows multiple InstanceIDs to be created, depending
 * on the provided subtype. Only for platforms-within-platforms like browsers.
 */
public class InstanceIDWithSubtype {
    // Must match the private InstanceID.OPTION_SUBTYPE, which is guaranteed to not change.
    private static final String OPTION_SUBTYPE = "subtype";

    private final InstanceID mInstanceID;

    /**
     * Cached instances. May be accessed from multiple threads; synchronize on sSubtypeInstancesLock
     */
    @VisibleForTesting
    protected static final Map<String, InstanceIDWithSubtype> sSubtypeInstances = new HashMap<>();
    protected static final Object sSubtypeInstancesLock = new Object();

    /** Fake subclasses can set this so getInstance creates instances of them. */
    @VisibleForTesting
    protected static FakeFactory sFakeFactoryForTesting;

    protected InstanceIDWithSubtype(InstanceID instanceID) {
        mInstanceID = instanceID;
    }

    /**
     * Returns an instance of this class. Unlike {@link InstanceID#getInstance(Context)}, it is not
     * a singleton, but instead a different instance will be returned for each {@code subtype}.
     */
    public static InstanceIDWithSubtype getInstance(String subtype) {
        if (TextUtils.isEmpty(subtype)) {
            throw new IllegalArgumentException("subtype must not be empty");
        }

        synchronized (sSubtypeInstancesLock) {
            InstanceIDWithSubtype existing = sSubtypeInstances.get(subtype);
            if (existing == null) {
                if (sFakeFactoryForTesting != null) {
                    existing = sFakeFactoryForTesting.create(subtype);
                } else {
                    Bundle options = new Bundle();
                    options.putCharSequence(OPTION_SUBTYPE, subtype);
                    InstanceID instanceID =
                            InstanceID.getInstance(ContextUtils.getApplicationContext(), options);
                    existing = new InstanceIDWithSubtype(instanceID);
                }
                sSubtypeInstances.put(subtype, existing);
            }
            return existing;
        }
    }

    public String getSubtype() {
        return mInstanceID.getSubtype();
    }

    public String getId() {
        return mInstanceID.getId();
    }

    public long getCreationTime() {
        return mInstanceID.getCreationTime();
    }

    public void deleteInstanceID() throws IOException {
        synchronized (sSubtypeInstancesLock) {
            sSubtypeInstances.remove(mInstanceID.getSubtype());
            mInstanceID.deleteInstanceID();
        }
    }

    public void deleteToken(String authorizedEntity, String scope) throws IOException {
        mInstanceID.deleteToken(authorizedEntity, scope);
    }

    public String getToken(String authorizedEntity, String scope) throws IOException {
        return mInstanceID.getToken(authorizedEntity, scope);
    }

    public String getToken(String authorizedEntity, String scope, Bundle extras)
            throws IOException {
        return mInstanceID.getToken(authorizedEntity, scope, extras);
    }

    /** Fake subclasses can set {@link #sFakeFactoryForTesting} to an implementation of this. */
    @VisibleForTesting
    public interface FakeFactory {
        public InstanceIDWithSubtype create(String subtype);
    }
}
