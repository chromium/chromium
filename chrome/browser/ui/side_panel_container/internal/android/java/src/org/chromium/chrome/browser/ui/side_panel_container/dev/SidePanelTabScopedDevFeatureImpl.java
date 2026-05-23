// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.side_panel_container.dev;

import android.annotation.SuppressLint;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.EditText;
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
    private static final String DEV_LINK_URL = "https://www.chromium.org";
    private static final String DEV_INPUT_HINT = "Enter some text";

    private static final String LOREM_IPSUM =
            """
            Lorem ipsum dolor sit amet, consectetur adipiscing elit. Sed do eiusmod tempor \
            incididunt ut labore et dolore magna aliqua. Ut enim ad minim veniam, quis nostrud \
            exercitation ullamco laboris nisi ut aliquip ex ea commodo consequat. Duis aute \
            irure dolor in reprehenderit in voluptate velit esse cillum dolore eu fugiat nulla \
            pariatur. Excepteur sint occaecat cupidatat non proident, sunt in culpa qui officia \
            deserunt mollit anim id est laborum.

            Morbi tincidunt, dui sit amet facilisis convallis, leo augue interdum magna, ac \
            convallis metus nisl eget ante. Phasellus ac turpis arcu. In convallis porta dictum. \
            Integer tristique, augue ac pellentesque semper, arcu turpis efficitur nisl, quis \
            tincidunt magna lacus eu elit. Mauris non tellus elementum, vehicula tellus eget, \
            feugiat lectus. Ut a nisl vel mauris condimentum hendrerit. Vestibulum ante ipsum \
            primis in faucibus orci luctus et ultrices posuere cubilia curae; Cras eget hendrerit \
            elit, ac congue lorem.\
            """;

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

        TextView linkTextView = view.findViewById(R.id.dev_link);
        linkTextView.setText(DEV_LINK_URL);

        TextView fillerTextView = view.findViewById(R.id.filler_text);
        fillerTextView.setText(LOREM_IPSUM);

        EditText inputEditText = view.findViewById(R.id.dev_input);
        inputEditText.setHint(DEV_INPUT_HINT);

        return view;
    }

    @NativeMethods
    public interface Natives {
        void toggleTabScopedDevFeature(@JniType("TabAndroid*") Tab tab);
    }
}
