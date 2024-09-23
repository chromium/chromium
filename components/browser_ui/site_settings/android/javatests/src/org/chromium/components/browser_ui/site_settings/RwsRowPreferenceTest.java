// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.site_settings;

import static org.junit.Assert.assertFalse;

import android.app.Activity;
import android.view.LayoutInflater;

import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.BeforeClass;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import org.chromium.base.library_loader.LibraryLoader;
import org.chromium.base.library_loader.LibraryProcessType;
import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.components.browser_ui.settings.BlankUiTestActivitySettingsTestRule;
import org.chromium.components.browser_ui.settings.PlaceholderSettingsForTest;

/** Tests for WebsiteRowPreference. */
@RunWith(BaseJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
public class RwsRowPreferenceTest {
    @Rule
    public final BlankUiTestActivitySettingsTestRule mSettingsRule =
            new BlankUiTestActivitySettingsTestRule();

    private Activity mActivity;

    @Mock private SiteSettingsDelegate mDelegate;

    @BeforeClass
    public static void setupSuite() {
        LibraryLoader.getInstance().setLibraryProcessType(LibraryProcessType.PROCESS_BROWSER);
        LibraryLoader.getInstance().ensureInitialized();
    }

    @Before
    public void setUp() {
        MockitoAnnotations.openMocks(this);
        mSettingsRule.launchPreference(PlaceholderSettingsForTest.class);
        mActivity = mSettingsRule.getActivity();
    }

    @Test
    @SmallTest
    public void testRwsRowPreferenceFocus() {
        Website website = new Website(WebsiteAddress.create("https://test.com"), null);
        RwsRowPreference rwsRowPreference =
                new RwsRowPreference(mActivity, mDelegate, website, LayoutInflater.from(mActivity));

        assertFalse(rwsRowPreference.isSelectable());
    }
}
