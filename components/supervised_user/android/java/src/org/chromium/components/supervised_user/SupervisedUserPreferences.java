// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.supervised_user;

import org.jni_zero.NativeMethods;

import org.chromium.components.prefs.PrefService;

/** Provides access to selected supervised users preferences. */
public class SupervisedUserPreferences {
    /** Returns true if a supervised user account is signed in. */
    public static boolean isSubjectToParentalControls(PrefService prefService) {
        return SupervisedUserPreferencesJni.get().isSubjectToParentalControls(prefService);
    }

    @NativeMethods
    public interface Natives {
        boolean isSubjectToParentalControls(PrefService prefService);
    }
}
