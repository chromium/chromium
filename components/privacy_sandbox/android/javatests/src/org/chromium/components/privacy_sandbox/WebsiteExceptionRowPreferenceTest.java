// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.privacy_sandbox;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.hamcrest.CoreMatchers.containsString;
import static org.hamcrest.Matchers.allOf;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import static org.chromium.ui.test.util.ViewUtils.onViewWaiting;

import android.app.Activity;
import android.graphics.drawable.Drawable;

import androidx.appcompat.content.res.AppCompatResources;
import androidx.preference.PreferenceScreen;
import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.BeforeClass;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.MockitoAnnotations;
import org.mockito.stubbing.Answer;

import org.chromium.base.Callback;
import org.chromium.base.library_loader.LibraryLoader;
import org.chromium.base.library_loader.LibraryProcessType;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.DoNotBatch;
import org.chromium.components.browser_ui.settings.BlankUiTestActivitySettingsTestRule;
import org.chromium.components.browser_ui.settings.PlaceholderSettingsForTest;
import org.chromium.components.browser_ui.site_settings.ContentSettingException;
import org.chromium.components.browser_ui.site_settings.SiteSettingsDelegate;
import org.chromium.components.browser_ui.site_settings.Website;
import org.chromium.components.browser_ui.site_settings.WebsiteAddress;
import org.chromium.components.browser_ui.site_settings.WebsitePreferenceBridge;
import org.chromium.components.browser_ui.site_settings.WebsitePreferenceBridgeJni;
import org.chromium.components.content_settings.ContentSetting;
import org.chromium.components.content_settings.ContentSettingsType;
import org.chromium.components.content_settings.ProviderType;
import org.chromium.components.privacy_sandbox.WebsiteExceptionRowPreference.WebsiteExceptionDeletedCallback;
import org.chromium.url.GURL;

/** Tests for WebsiteExceptionRowPreference. */
@RunWith(BaseJUnit4ClassRunner.class)
@DoNotBatch(reason = "Test start up behaviors.")
public class WebsiteExceptionRowPreferenceTest {
    @Rule
    public final BlankUiTestActivitySettingsTestRule mSettingsRule =
            new BlankUiTestActivitySettingsTestRule();

    @Mock private WebsitePreferenceBridge.Natives mBridgeMock;

    private Activity mActivity;
    private WebsiteExceptionRowPreference mPreference;
    private PreferenceScreen mPreferenceScreen;

    @Mock private TrackingProtectionDelegate mDelegate;

    @Mock private SiteSettingsDelegate mSiteSettingsDelegate;

    @Mock private WebsiteExceptionDeletedCallback mCallback;

    private static final String TEST_URL_WITH_WILDCARD = "https://[*.]test.com";

    @BeforeClass
    public static void setupSuite() {
        LibraryLoader.getInstance().setLibraryProcessType(LibraryProcessType.PROCESS_BROWSER);
        LibraryLoader.getInstance().ensureInitialized();
    }

    @Before
    public void setUp() {
        MockitoAnnotations.openMocks(this);
        WebsitePreferenceBridgeJni.setInstanceForTesting(mBridgeMock);
        mSettingsRule.launchPreference(PlaceholderSettingsForTest.class);
        mPreferenceScreen = mSettingsRule.getPreferenceScreen();
        mActivity = mSettingsRule.getActivity();
        when(mDelegate.getSiteSettingsDelegate(Mockito.any())).thenReturn(mSiteSettingsDelegate);
    }

    @After
    public void tearDown() {
        mSettingsRule.getActivity().finish();
    }

    @Test
    @SmallTest
    public void createException_displayedCorrectly() {
        Website site = new Website(WebsiteAddress.create("https://test.com"), null);
        mPreference = new WebsiteExceptionRowPreference(mActivity, site, mDelegate, mCallback);
        mPreferenceScreen.addPreference(mPreference);
        // Check the title, summary, and the delete button.
        onViewWaiting(withId(android.R.id.title))
                .check(matches(allOf(withText("https://test.com"), isDisplayed())));
        onView(withId(android.R.id.summary))
                .check(matches(allOf(withText("Does not expire"), isDisplayed())));
        onView(withId(R.id.image_view_widget)).check(matches(isDisplayed()));
    }

    @Test
    @SmallTest
    public void createExceptionWithSecondaryPattern_displayedCorrectly() {
        Website site =
                new Website(
                        WebsiteAddress.create("*"), WebsiteAddress.create(TEST_URL_WITH_WILDCARD));
        site.setContentSettingException(
                ContentSettingsType.COOKIES,
                new ContentSettingException(
                        ContentSettingsType.COOKIES,
                        /* primaryPattern */ "*",
                        TEST_URL_WITH_WILDCARD,
                        ContentSetting.ALLOW,
                        ProviderType.PREF_PROVIDER,
                        /* expirationInDays= */ null,
                        /* isEmbargoed= */ false));
        mPreference = new WebsiteExceptionRowPreference(mActivity, site, mDelegate, mCallback);
        mPreferenceScreen.addPreference(mPreference);
        // Check the title, summary, and the delete button.
        onViewWaiting(withId(android.R.id.title))
                .check(matches(allOf(withText(TEST_URL_WITH_WILDCARD), isDisplayed())));
        onView(withId(android.R.id.summary))
                .check(matches(allOf(withText("Does not expire"), isDisplayed())));
        onView(withId(R.id.image_view_widget)).check(matches(isDisplayed()));
    }

    @Test
    @SmallTest
    public void createExceptionWithPrimaryPattern_displayedCorrectly() {
        Website site =
                new Website(
                        WebsiteAddress.create(TEST_URL_WITH_WILDCARD), WebsiteAddress.create("*"));
        site.setContentSettingException(
                ContentSettingsType.COOKIES,
                new ContentSettingException(
                        ContentSettingsType.COOKIES,
                        TEST_URL_WITH_WILDCARD,
                        /* secondaryPattern */ "*",
                        ContentSetting.ALLOW,
                        ProviderType.PREF_PROVIDER,
                        /* expirationInDays= */ null,
                        /* isEmbargoed= */ false));
        mPreference = new WebsiteExceptionRowPreference(mActivity, site, mDelegate, mCallback);
        mPreferenceScreen.addPreference(mPreference);
        // Check the title, summary, and the delete button.
        onViewWaiting(withId(android.R.id.title))
                .check(matches(allOf(withText(TEST_URL_WITH_WILDCARD), isDisplayed())));
        onView(withId(android.R.id.summary))
                .check(matches(allOf(withText("Does not expire"), isDisplayed())));
        onView(withId(R.id.image_view_widget)).check(matches(isDisplayed()));
    }

    @Test
    @SmallTest
    public void createExceptionWithExpiration_displayedCorrectly() {
        Website site = new Website(WebsiteAddress.create("https://test.com"), null);
        site.setContentSettingException(
                ContentSettingsType.COOKIES,
                new ContentSettingException(
                        ContentSettingsType.COOKIES,
                        site.getAddress().getOrigin(),
                        /* secondaryPattern= */ "*",
                        ContentSetting.ALLOW,
                        ProviderType.PREF_PROVIDER,
                        /* expirationInDays= */ 66,
                        /* isEmbargoed= */ false));
        mPreference = new WebsiteExceptionRowPreference(mActivity, site, mDelegate, mCallback);
        mPreferenceScreen.addPreference(mPreference);
        // Check the title, summary, and the delete button.
        onViewWaiting(withId(android.R.id.title))
                .check(matches(allOf(withText("https://test.com"), isDisplayed())));
        onView(withId(android.R.id.summary))
                .check(matches(allOf(withText(containsString("66 days")), isDisplayed())));
        onView(withId(R.id.image_view_widget)).check(matches(isDisplayed()));
    }

    @Test
    @SmallTest
    public void createExceptionWithExpirationToday_displayedCorrectly() {
        Website site = new Website(WebsiteAddress.create("https://test.com"), null);
        site.setContentSettingException(
                ContentSettingsType.COOKIES,
                new ContentSettingException(
                        ContentSettingsType.COOKIES,
                        site.getAddress().getOrigin(),
                        /* secondaryPattern= */ "*",
                        ContentSetting.ALLOW,
                        ProviderType.PREF_PROVIDER,
                        /* expirationInDays= */ 0,
                        /* isEmbargoed= */ false));
        mPreference = new WebsiteExceptionRowPreference(mActivity, site, mDelegate, mCallback);
        mPreferenceScreen.addPreference(mPreference);
        // Check the title, summary, and the delete button.
        onViewWaiting(withId(android.R.id.title))
                .check(matches(allOf(withText("https://test.com"), isDisplayed())));
        onView(withId(android.R.id.summary))
                .check(matches(allOf(withText(containsString("today")), isDisplayed())));
        onView(withId(R.id.image_view_widget)).check(matches(isDisplayed()));
    }

    @Test
    @SmallTest
    public void deleteException_triggersDeletionAndRefresh() {
        var site = new Website(WebsiteAddress.create("https://test.com"), null);
        var exception =
                new ContentSettingException(
                        ContentSettingsType.COOKIES,
                        site.getAddress().getOrigin(),
                        /* secondaryPattern= */ "*",
                        ContentSetting.ALLOW,
                        ProviderType.PREF_PROVIDER,
                        /* expirationInDays= */ null,
                        /* isEmbargoed= */ false);
        site.setContentSettingException(ContentSettingsType.COOKIES, exception);
        mPreference = new WebsiteExceptionRowPreference(mActivity, site, mDelegate, mCallback);
        mPreferenceScreen.addPreference(mPreference);
        onViewWaiting(withId(R.id.image_view_widget)).perform(click());
        // Check the content setting is reset to default.
        verify(mBridgeMock)
                .setContentSettingCustomScope(
                        /* browserContextHandle= */ Mockito.any(),
                        Mockito.eq(ContentSettingsType.COOKIES),
                        Mockito.eq(site.getAddress().getOrigin()),
                        /* secondaryPattern= */ Mockito.eq("*"),
                        Mockito.eq(ContentSetting.DEFAULT));
        // Check the refresh callback is triggered.
        verify(mCallback).refreshBlockingExceptions();
    }

    @Test
    @SmallTest
    public void fetchFavicon_displaysFavicon() {
        doAnswer(
                        (Answer<Void>)
                                a -> {
                                    var callback = (Callback<Drawable>) a.getArguments()[1];
                                    PostTask.postTask(
                                            TaskTraits.UI_DEFAULT,
                                            () -> {
                                                Drawable icon =
                                                        AppCompatResources.getDrawable(
                                                                mActivity,
                                                                android.R.drawable
                                                                        .sym_def_app_icon);
                                                callback.onResult(icon);
                                            });
                                    return null;
                                })
                .when(mSiteSettingsDelegate)
                .getFaviconImageForURL(Mockito.any(), Mockito.any());
        Website site = new Website(WebsiteAddress.create("https://test.com"), null);
        mPreference = new WebsiteExceptionRowPreference(mActivity, site, mDelegate, mCallback);
        mPreferenceScreen.addPreference(mPreference);
        // Wait until the preference is displayed.
        onViewWaiting(withId(android.R.id.title))
                .check(matches(allOf(withText("https://test.com"), isDisplayed())));
        onViewWaiting(withId(android.R.id.icon)).check(matches(isDisplayed()));
    }

    @Test
    @SmallTest
    public void fetchFaviconSubdomain_returnsFaviconUrl() {
        doAnswer(
                        (Answer<Void>)
                                a -> {
                                    var url = (GURL) a.getArguments()[0];
                                    Assert.assertEquals("http://test.com/", url.getSpec());
                                    return null;
                                })
                .when(mSiteSettingsDelegate)
                .getFaviconImageForURL(Mockito.any(), Mockito.any());
        Website site = new Website(WebsiteAddress.create("[*.]test.com"), null);
        mPreference = new WebsiteExceptionRowPreference(mActivity, site, mDelegate, mCallback);
        mPreferenceScreen.addPreference(mPreference);
        // Wait until the preference is displayed.
        onViewWaiting(withId(android.R.id.title)).check(matches(isDisplayed()));
        verify(mSiteSettingsDelegate, times(1)).getFaviconImageForURL(Mockito.any(), Mockito.any());
    }
}
