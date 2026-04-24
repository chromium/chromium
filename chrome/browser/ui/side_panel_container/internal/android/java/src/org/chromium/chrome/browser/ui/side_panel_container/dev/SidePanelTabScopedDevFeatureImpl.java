// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.side_panel_container.dev;

import android.annotation.SuppressLint;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.TextView;

import org.jni_zero.CalledByNative;
import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.base.ThreadUtils;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.ui.side_panel_container.R;

import java.util.function.Supplier;

/** Implements a tab scoped {@link SidePanelDevFeature}. */
@NullMarked
public final class SidePanelTabScopedDevFeatureImpl implements SidePanelDevFeature {
    private final Supplier<Tab> mTabSupplier;

    /**
     * Constructor for {@link SidePanelTabScopedDevFeatureImpl}.
     *
     * @param tabSupplier Supplier for the current tab.
     */
    public SidePanelTabScopedDevFeatureImpl(Supplier<Tab> tabSupplier) {
        mTabSupplier = tabSupplier;
    }

    @Override
    public void toggle() {
        ThreadUtils.assertOnUiThread();
        Tab tab = mTabSupplier.get();
        if (tab != null) {
            SidePanelTabScopedDevFeatureImplJni.get().toggleTabScopedDevFeature(tab);
        }
    }

    @Override
    public void destroy() {
        ThreadUtils.assertOnUiThread();
    }

    @SuppressLint("SetTextI18n")
    @CalledByNative
    private static View createTabScopedView(Tab tab) {
        View view =
                LayoutInflater.from(tab.getContext())
                        .inflate(R.layout.side_panel_dev_feature, null);
        TextView textView = view.findViewById(R.id.tab_title);
        String hash = Integer.toHexString(System.identityHashCode(tab));
        textView.setText("Tab title: " + tab.getTitle() + " " + hash);

        return view;
    }

    @NativeMethods
    public interface Natives {
        void toggleTabScopedDevFeature(@JniType("TabAndroid*") Tab tab);
    }
}
