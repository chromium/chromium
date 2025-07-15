// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.extensions;

import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.Activity;

import com.google.android.material.divider.MaterialDivider;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;

import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabCreator;
import org.chromium.chrome.browser.theme.ThemeColorProvider;
import org.chromium.chrome.browser.ui.extensions.FakeExtensionActionsBridge;
import org.chromium.chrome.browser.ui.extensions.FakeExtensionActionsBridge.ProfileModel;
import org.chromium.chrome.browser.ui.extensions.FakeExtensionActionsBridgeRule;
import org.chromium.ui.listmenu.ListMenuButton;

/** Unit tests for {@link ExtensionsMenuButtonCoordinator}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class ExtensionsMenuButtonCoordinatorTest {
    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private Profile mProfile;
    @Mock private Tab mTab;
    @Mock private Tab mNewTab;
    @Mock private ThemeColorProvider mThemeColorProvider;
    @Mock private TabCreator mTabCreator;

    private final ObservableSupplierImpl<Profile> mProfileSupplier = new ObservableSupplierImpl<>();
    private final ObservableSupplierImpl<Tab> mTabSupplier = new ObservableSupplierImpl<>();

    private ExtensionsMenuButtonCoordinator mCoordinator;
    private Activity mActivity;
    private ListMenuButton mExtensionsMenuButton;
    private MaterialDivider mExtensionsMenuTabSwitcherDivider;

    @Rule
    public final FakeExtensionActionsBridgeRule mBridgeRule = new FakeExtensionActionsBridgeRule();

    private final FakeExtensionActionsBridge mBridge = mBridgeRule.getFakeBridge();
    private ProfileModel mProfileModel;

    @Before
    public void setUp() {
        mActivity = Robolectric.buildActivity(Activity.class).setup().get();
        mExtensionsMenuButton = new ListMenuButton(mActivity, null);
        mExtensionsMenuTabSwitcherDivider = new MaterialDivider(mActivity);

        mProfileModel = mBridge.getOrCreateProfileModel(mProfile);
        mProfileModel.setInitialized(true);

        when(mTab.getProfile()).thenReturn(mProfile);
        when(mNewTab.getProfile()).thenReturn(mProfile);

        mProfileSupplier.set(mProfile);
        mTabSupplier.set(mTab);

        mCoordinator =
                new ExtensionsMenuButtonCoordinator(
                        mActivity,
                        mExtensionsMenuButton,
                        mExtensionsMenuTabSwitcherDivider,
                        mThemeColorProvider,
                        mProfileSupplier,
                        mTabSupplier,
                        mTabCreator);
    }

    @Test
    public void testMenuDestroyedOnTabChange() {
        // TODO(crbug.com/431915409): Remove this test once we provided the ability to keep
        // the menu open during tab changes.
        // Set the profile.
        mProfileSupplier.set(null);
        mProfileSupplier.set(mProfile);

        // Show the menu.
        mCoordinator.onClick(mExtensionsMenuButton);

        // Spy on the created menu coordinator.
        ExtensionsMenuCoordinator menuCoordinatorSpy = spy(mCoordinator.mExtensionsMenuCoordinator);
        mCoordinator.mExtensionsMenuCoordinator = menuCoordinatorSpy;

        // Change the tab.
        mTabSupplier.set(mNewTab);

        // Verify the menu is destroyed.
        verify(menuCoordinatorSpy).destroy();
    }
}
