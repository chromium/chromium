// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.cronet_sample_apk;

import androidx.fragment.app.Fragment;
import androidx.lifecycle.ViewModel;

import java.util.HashMap;
import java.util.Map;

public class SampleActivityViewModel extends ViewModel {
    private Map<Integer, Fragment> mFragmentMap = new HashMap<>();

    public static final int FRAGMENT_ID_HOME = 0;
    public static final int FRAGMENT_ID_FLAGS = 1;

    public Fragment getFragment(int fragmentId) {
        if (mFragmentMap.containsKey(fragmentId)) {
            return mFragmentMap.get(fragmentId);
        }
        Fragment fragment;
        switch (fragmentId) {
            case FRAGMENT_ID_HOME:
                fragment = new MainFragment();
                break;
            case FRAGMENT_ID_FLAGS:
                fragment = new OptionsFragment();
                break;
            default:
                throw new IllegalArgumentException(
                        String.format(
                                "Fragment %d does not map to any implementation.", fragmentId));
        }
        mFragmentMap.put(fragmentId, fragment);
        return fragment;
    }
}
