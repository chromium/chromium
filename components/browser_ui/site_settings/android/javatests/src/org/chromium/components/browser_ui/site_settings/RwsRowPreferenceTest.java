// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.site_settings;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withId;

import static org.hamcrest.CoreMatchers.not;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;

import android.app.Activity;
import android.view.LayoutInflater;

import androidx.preference.PreferenceScreen;
import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.BeforeClass;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.MockitoAnnotations;

import org.chromium.base.library_loader.LibraryLoader;
import org.chromium.base.library_loader.LibraryProcessType;
import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.components.browser_ui.settings.BlankUiTestActivitySettingsTestRule;
import org.chromium.components.browser_ui.settings.PlaceholderSettingsForTest;

import java.util.List;

/** Tests for WebsiteRowPreference. */
@RunWith(BaseJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
public class RwsRowPreferenceTest {
    @Rule
    public final BlankUiTestActivitySettingsTestRule mSettingsRule =
            new BlankUiTestActivitySettingsTestRule();

    private Activity mActivity;
    private PreferenceScreen mPreferenceScreen;

    @Mock private SiteSettingsDelegate mDelegate;

    @BeforeClass
    public static void setupSuite() {
        LibraryLoader.getInstance().setLibraryProcessType(LibraryProcessType.PROCESS_BROWSER);
        LibraryLoader.getInstance().ensureInitialized();
    }

    @Before
    public void setUp() {
        MockitoAnnotations.openMocks(this);
        // Enable RWS V2 UI for all tests
        Mockito.doReturn(true).when(mDelegate).shouldShowPrivacySandboxRwsUi();
        mSettingsRule.launchPreference(PlaceholderSettingsForTest.class);
        mActivity = mSettingsRule.getActivity();
        mPreferenceScreen = mSettingsRule.getPreferenceScreen();
    }

    @Test
    @SmallTest
    public void testRwsRowPreferenceFocus() {
        Website website = new Website(WebsiteAddress.create("https://test.com"), null);
        RwsRowPreference rwsRowPreference =
                new RwsRowPreference(mActivity, mDelegate, website, LayoutInflater.from(mActivity));

        assertFalse(rwsRowPreference.isSelectable());
    }

    @Test
    @SmallTest
    public void rwsMembershipLabelNotShown() {
        Website origin1 = new Website(WebsiteAddress.create("https://one.test.com"), null);
        Website origin2 = new Website(WebsiteAddress.create("https://two.test.com"), null);
        RwsCookieInfo rwsInfo =
                new RwsCookieInfo(
                        origin1.getAddress().getDomainAndRegistry(), List.of(origin1, origin2));
        origin1.setRwsCookieInfo(rwsInfo);
        origin1.setCookiesInfo(new CookiesInfo(5));
        RwsRowPreference rwsRowPreference =
                new RwsRowPreference(mActivity, mDelegate, origin1, LayoutInflater.from(mActivity));

        assertEquals("5 cookies", rwsRowPreference.getSummary().toString());
    }

    @Test
    @SmallTest
    public void deleteIconShown() {
        Website website = new Website(WebsiteAddress.create("https://test.com"), null);
        RwsRowPreference rwsRowPreference =
                new RwsRowPreference(mActivity, mDelegate, website, LayoutInflater.from(mActivity));
        mPreferenceScreen.addPreference(rwsRowPreference);
        onView(withId(R.id.image_view_widget)).check(matches(isDisplayed()));
    }

    // TODO(crbug.com/396463421): Remove once RWS UI V2 launched.
    @Test
    @SmallTest
    public void deleteIconNotShownWhenRwsV2Disabled() {
        Mockito.doReturn(false).when(mDelegate).shouldShowPrivacySandboxRwsUi();
        Website website = new Website(WebsiteAddress.create("https://test.com"), null);
        RwsRowPreference rwsRowPreference =
                new RwsRowPreference(mActivity, mDelegate, website, LayoutInflater.from(mActivity));
        mPreferenceScreen.addPreference(rwsRowPreference);
        onView(withId(R.id.image_view_widget)).check(matches(not(isDisplayed())));
    }
}
