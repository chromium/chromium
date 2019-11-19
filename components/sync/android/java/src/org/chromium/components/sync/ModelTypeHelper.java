// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.sync;

import androidx.annotation.VisibleForTesting;

import com.google.ipc.invalidation.external.client.types.ObjectId;
import com.google.protos.ipc.invalidation.Types;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;

import java.util.Collection;
import java.util.HashSet;
import java.util.Set;

/**
 * Helper methods for dealing with ModelTypes.
 *
 * This class deals primarily with converting ModelTypes into notification types (string
 * representations that are used to register for invalidations) and converting notification
 * types into the actual ObjectIds used for invalidations.
 *
 */
@JNINamespace("syncer")
public class ModelTypeHelper {
    /**
     * Implement this class to override the behavior of
     * {@link ModelTypeHelper#toNotificationType()} for tests.
     */
    public interface TestDelegate { public String toNotificationType(int modelType); }

    private static final String TAG = "ModelTypeHelper";

    private static final Object sLock = new Object();

    private static final int[] NON_INVALIDATION_TYPES_ARRAY = new int[] {ModelType.PROXY_TABS};

    private static TestDelegate sDelegate;

    // Convenience sets for checking whether a type can have invalidations. Some ModelTypes
    // such as PROXY_TABS are not real types and can't be registered. Initializing these
    // once reduces toNotificationType() calls in the isInvalidationType() method.
    private static Set<String> sNonInvalidationTypes;

    /**
     * Initializes the non-invalidation sets. Called lazily the first time they're needed.
     */
    private static void initNonInvalidationTypes() {
        synchronized (sLock) {
            if (sNonInvalidationTypes != null) return;

            sNonInvalidationTypes = new HashSet<String>();
            for (int i = 0; i < NON_INVALIDATION_TYPES_ARRAY.length; i++) {
                sNonInvalidationTypes.add(toNotificationType(NON_INVALIDATION_TYPES_ARRAY[i]));
            }
        }
    }

    /**
     * Checks whether a type is allowed to register for invalidations.
     */
    private static boolean isInvalidationType(String notificationType) {
        initNonInvalidationTypes();
        return !sNonInvalidationTypes.contains(notificationType);
    }

    /**
     * Converts a notification type into an ObjectId.
     *
     * If the model type is not an invalidation type, this function uses the string "NULL".
     */
    private static ObjectId toObjectId(String notificationType) {
        String objectIdString = isInvalidationType(notificationType) ? notificationType : "NULL";
        return ObjectId.newInstance(
                Types.ObjectSource.CHROME_SYNC, ApiCompatibilityUtils.getBytesUtf8(objectIdString));
    }

    @VisibleForTesting
    public static ObjectId toObjectId(int modelType) {
        return toObjectId(toNotificationType(modelType));
    }

    /**
     * Converts a model type to its notification type representation using JNI.
     *
     * This is the value that is stored in the invalidation preferences and used to
     * register for invalidations.
     *
     * @param modelType the model type to convert to a string.
     * @return the string representation of the model type constant.
     */
    public static String toNotificationType(int modelType) {
        if (sDelegate != null) return sDelegate.toNotificationType(modelType);

        // Because PROXY_TABS isn't an invalidation type, it doesn't have a string from native,
        // but for backwards compatibility we need to keep its pref value the same as the old
        // ModelType enum name value.
        if (modelType == ModelType.PROXY_TABS) {
            return "PROXY_TABS";
        }
        return ModelTypeHelperJni.get().modelTypeToNotificationType(modelType);
    }

    /**
     * Converts a set of {@link String} notification types to a set of {@link ObjectId}.
     *
     * This function assumes that all the strings passed in were generated with
     * ModelTypeHelper.toNotificationType. Any notification types that are nonInvalidationTypes
     * are filtered out.
     */
    public static Set<ObjectId> notificationTypesToObjectIds(Collection<String> notificationTypes) {
        Set<ObjectId> objectIds = new HashSet<ObjectId>();
        for (String notificationType : notificationTypes) {
            if (isInvalidationType(notificationType)) {
                objectIds.add(toObjectId(notificationType));
            }
        }
        return objectIds;
    }

    @VisibleForTesting
    public static void setTestDelegate(TestDelegate delegate) {
        sDelegate = delegate;
    }

    @NativeMethods
    interface Natives {
        String modelTypeToNotificationType(int modelType);
    }
}
