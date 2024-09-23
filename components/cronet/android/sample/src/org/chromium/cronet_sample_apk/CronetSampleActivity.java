// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.cronet_sample_apk;

import static org.chromium.cronet_sample_apk.SampleActivityViewModel.FRAGMENT_ID_FLAGS;
import static org.chromium.cronet_sample_apk.SampleActivityViewModel.FRAGMENT_ID_HOME;

import android.graphics.PorterDuff;
import android.graphics.PorterDuffColorFilter;
import android.graphics.drawable.Drawable;
import android.os.Bundle;
import android.view.View;
import android.widget.LinearLayout;
import android.widget.TextView;

import androidx.fragment.app.FragmentActivity;
import androidx.lifecycle.ViewModelProvider;

import java.util.HashMap;
import java.util.Map;

/** Activity for managing the Cronet Sample. */
public class CronetSampleActivity extends FragmentActivity {
    private static final String TAG = CronetSampleActivity.class.getSimpleName();

    private final Map<Integer, Integer> mFragmentIdMap = new HashMap<>();

    private LinearLayout mBottomNav;
    private SampleActivityViewModel mActivityViewModel;

    private void init() {
        setContentView(R.layout.main_activity);
        // Set up bottom navigation bar:
        mBottomNav = findViewById(R.id.nav_view);
        mFragmentIdMap.put(R.id.navigation_home, FRAGMENT_ID_HOME);
        mFragmentIdMap.put(R.id.navigation_options_ui, FRAGMENT_ID_FLAGS);
        final int childCount = mBottomNav.getChildCount();
        View.OnClickListener switchFragmentListener =
                view -> {
                    assert mFragmentIdMap.containsKey(view.getId());
                    int fragmentId = mFragmentIdMap.get(view.getId());
                    switchFragment(fragmentId);
                };
        for (int i = 0; i < childCount; i++) {
            mBottomNav.getChildAt(i).setOnClickListener(switchFragmentListener);
        }
        mActivityViewModel = new ViewModelProvider(this).get(SampleActivityViewModel.class);
    }

    @Override
    protected void onCreate(final Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        init();
        switchFragment(FRAGMENT_ID_HOME);
    }

    @SuppressWarnings("deprecation")
    private void updateNavigationBarUI(int chosenFragmentId) {
        final int childCount = mBottomNav.getChildCount();
        for (int i = 0; i < childCount; ++i) {
            View view = mBottomNav.getChildAt(i);
            int fragmentId = mFragmentIdMap.get(view.getId());
            assert view instanceof TextView : "Bottom bar must have TextViews as direct children";
            TextView textView = (TextView) view;

            boolean isSelectedFragment = chosenFragmentId == fragmentId;
            // TODO: Can remove first parameter and SuppressWarnings once minApiLevel >= 23.
            textView.setTextAppearance(
                    textView.getContext(),
                    isSelectedFragment
                            ? R.style.SelectedNavigationButton
                            : R.style.UnselectedNavigationButton);
            int color =
                    isSelectedFragment
                            ? getColor(R.color.navigation_selected)
                            : getColor(R.color.navigation_unselected);
            for (Drawable drawable : textView.getCompoundDrawables()) {
                if (drawable != null) {
                    drawable.mutate();
                    drawable.setColorFilter(
                            new PorterDuffColorFilter(color, PorterDuff.Mode.SRC_IN));
                }
            }
        }
    }

    private void switchFragment(int chosenFragmentId) {
        getSupportFragmentManager()
                .beginTransaction()
                .replace(R.id.fragment_container, mActivityViewModel.getFragment(chosenFragmentId))
                .commit();
        updateNavigationBarUI(chosenFragmentId);
    }
}
