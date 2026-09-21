// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.searchactivityutils;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.clearInvocations;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.reset;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoMoreInteractions;

import static org.chromium.chrome.browser.preferences.ChromePreferenceKeys.SEARCH_WIDGET_ACCOUNT_EMAIL;
import static org.chromium.chrome.browser.preferences.ChromePreferenceKeys.SEARCH_WIDGET_IS_AI_MODE_AVAILABLE;
import static org.chromium.chrome.browser.preferences.ChromePreferenceKeys.SEARCH_WIDGET_IS_GOOGLE_LENS_AVAILABLE;
import static org.chromium.chrome.browser.preferences.ChromePreferenceKeys.SEARCH_WIDGET_IS_INCOGNITO_AVAILABLE;
import static org.chromium.chrome.browser.preferences.ChromePreferenceKeys.SEARCH_WIDGET_IS_VOICE_SEARCH_AVAILABLE;
import static org.chromium.chrome.browser.preferences.ChromePreferenceKeys.SEARCH_WIDGET_SEARCH_ENGINE_SHORTNAME;
import static org.chromium.chrome.browser.preferences.ChromePreferenceKeys.SEARCH_WIDGET_SEARCH_ENGINE_URL;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.ContextUtils;
import org.chromium.base.shared_preferences.SharedPreferencesManager;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.RobolectricUtil;
import org.chromium.chrome.browser.composeplate.ComposeplateUtils;
import org.chromium.chrome.browser.incognito.IncognitoUtils;
import org.chromium.chrome.browser.lens.LensController;
import org.chromium.chrome.browser.omnibox.voice.VoiceRecognitionUtil;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.chrome.browser.search_engines.TemplateUrlServiceFactory;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.components.search_engines.TemplateUrl;
import org.chromium.components.search_engines.TemplateUrlService;
import org.chromium.components.search_engines.TemplateUrlService.LoadListener;
import org.chromium.components.search_engines.TemplateUrlService.TemplateUrlServiceObserver;
import org.chromium.components.signin.identitymanager.IdentityManager;
import org.chromium.url.GURL;

import java.util.function.Consumer;

/** Tests for {@link SearchActivityPreferencesManager}. */
@RunWith(BaseRobolectricTestRunner.class)
public class SearchActivityPreferencesManagerTest {
    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Mock private TemplateUrlService mTemplateUrlServiceMock;
    @Mock private TemplateUrl mTemplateUrlMock;
    @Mock private Profile mProfile;
    @Mock private LensController mLensController;
    @Mock private IdentityManager mIdentityManager;

    private LoadListener mTemplateUrlServiceLoadListener;
    private TemplateUrlServiceObserver mTemplateUrlServiceObserver;
    private SearchActivityPreferences mPreferences;

    @SuppressWarnings("unchecked") // mock() of generic Consumer type.
    private static Consumer<SearchActivityPreferences> mockPrefsConsumer() {
        return (Consumer<SearchActivityPreferences>) mock(Consumer.class);
    }

    // Typed wrapper around Mockito.clearInvocations() — @SafeVarargs avoids the
    // unchecked generic-array creation warning at every call site.
    @SafeVarargs
    private static void clearPrefsConsumerInvocations(
            Consumer<SearchActivityPreferences>... mocks) {
        clearInvocations(mocks);
    }

    @Before
    public void setUp() {
        LensController.setInstanceForTesting(mLensController);
        TemplateUrlServiceFactory.setInstanceForTesting(mTemplateUrlServiceMock);
        ProfileManager.setLastUsedProfileForTesting(mProfile);
        IdentityServicesProvider.setIdentityManagerForTesting(mIdentityManager);
        ComposeplateUtils.setIsEnabledForTesting(false);

        mPreferences =
                new SearchActivityPreferences.Builder()
                        .setAccountEmail("persisted@email.com")
                        .setSearchEngineName("Search Engine")
                        .setSearchEngineUrl(new GURL("https://URL"))
                        .setVoiceSearchAvailable(false)
                        .setGoogleLensAvailable(true)
                        .setAiModeAvailable(true)
                        .build();

        doAnswer(
                        invocation -> {
                            mTemplateUrlServiceLoadListener =
                                    (LoadListener) invocation.getArguments()[0];
                            return null;
                        })
                .when(mTemplateUrlServiceMock)
                .registerLoadListener(any());

        doAnswer(
                        invocation -> {
                            mTemplateUrlServiceObserver =
                                    (TemplateUrlServiceObserver) invocation.getArguments()[0];
                            return null;
                        })
                .when(mTemplateUrlServiceMock)
                .addObserver(any());

        SearchActivityPreferencesManager.resetForTesting();
        // Reset any cached values so we consistently start with a predictable state.
        SearchActivityPreferencesManager.resetCachedValues();

        // Make sure there were no premature attempts to register observers.
        Assert.assertNull(mTemplateUrlServiceLoadListener);
        Assert.assertNull(mTemplateUrlServiceObserver);

        // Purge any pending propagate actions to ensure no side effets later in the tests.
        // Needed because `resetCachedValues()` will likely post a task to notify listeners.
        RobolectricUtil.runAllBackgroundAndUi();
    }

    @After
    public void tearDown() {
        RobolectricUtil.runAllBackgroundAndUi();
        TemplateUrlServiceFactory.setInstanceForTesting(null);
        ProfileManager.setLastUsedProfileForTesting(null);
        SearchActivityPreferencesManager.resetForTesting();
    }

    @Test
    public void managerTest_updateIsPropagatedToAllObservers() {
        Consumer<SearchActivityPreferences> observer1 = mockPrefsConsumer();
        Consumer<SearchActivityPreferences> observer2 = mockPrefsConsumer();

        // Add 2 distinct listeners and confirm everybody gets called immediately with initial
        // values.
        SearchActivityPreferencesManager.addObserver(observer1);
        verify(observer1).accept(any());
        SearchActivityPreferencesManager.addObserver(observer2);
        verify(observer1).accept(any());
        clearPrefsConsumerInvocations(observer1, observer2);

        // Perform an update and check the number of calls.
        SearchActivityPreferencesManager.setCurrentlyLoadedPreferences(mPreferences, false);
        RobolectricUtil.runAllBackgroundAndUiIncludingDelayed();
        verify(observer1).accept(eq(mPreferences));
        verify(observer2).accept(eq(mPreferences));
        clearPrefsConsumerInvocations(observer1, observer2);

        // Add a new listener.
        Consumer<SearchActivityPreferences> observer3 = mockPrefsConsumer();
        SearchActivityPreferencesManager.addObserver(observer3);
        verify(observer3).accept(eq(mPreferences));
        clearPrefsConsumerInvocations(observer1, observer2, observer3);

        // Perform an update and check the number of calls.
        SearchActivityPreferences newPreferences =
                mPreferences.toBuilder().setVoiceSearchAvailable(true).build();
        SearchActivityPreferencesManager.setCurrentlyLoadedPreferences(newPreferences, false);
        RobolectricUtil.runAllBackgroundAndUiIncludingDelayed();
        verify(observer1).accept(eq(newPreferences));
        verify(observer2).accept(eq(newPreferences));
        verify(observer3).accept(eq(newPreferences));
        clearPrefsConsumerInvocations(observer1, observer2, observer3);

        // Finally, reset settings to safe defaults. All listeners should be notified.
        SearchActivityPreferencesManager.resetCachedValues();
        RobolectricUtil.runAllBackgroundAndUiIncludingDelayed();
        verify(observer1).accept(any());
        verify(observer2).accept(any());
        verify(observer3).accept(any());
    }

    @Test
    public void managerTest_eachObserverCanOnlyBeAddedOnce() {
        final Consumer<SearchActivityPreferences> listener1 = mockPrefsConsumer();

        // Add same listener a few times.
        SearchActivityPreferencesManager.addObserver(listener1);
        verify(listener1).accept(any());
        clearPrefsConsumerInvocations(listener1);

        SearchActivityPreferencesManager.addObserver(listener1);
        verify(listener1, never()).accept(any());

        // Add a different listener.
        Consumer<SearchActivityPreferences> listener2 = mockPrefsConsumer();
        SearchActivityPreferencesManager.addObserver(listener2);
        verify(listener1, never()).accept(any());
        verify(listener2).accept(any());
        clearPrefsConsumerInvocations(listener1, listener2);

        SearchActivityPreferencesManager.addObserver(listener2);
        SearchActivityPreferencesManager.addObserver(listener1);
        verify(listener1, never()).accept(any());
        verify(listener2, never()).accept(any());

        // Verify that we don't get excessive update notifications.
        SearchActivityPreferencesManager.setCurrentlyLoadedPreferences(mPreferences, false);
        verify(listener1, never()).accept(any());
        verify(listener2, never()).accept(any());
        RobolectricUtil.runAllBackgroundAndUi();
        verify(listener1).accept(any());
        verify(listener2).accept(any());
        clearPrefsConsumerInvocations(listener1, listener2);

        // Finally, confirm reset.
        SearchActivityPreferencesManager.resetCachedValues();
        verify(listener1, never()).accept(any());
        verify(listener2, never()).accept(any());
        RobolectricUtil.runAllBackgroundAndUi();
        verify(listener1).accept(any());
        verify(listener2).accept(any());
    }

    @Test
    public void managerTest_preferencesRetentionTest() {
        final SharedPreferencesManager manager = ChromeSharedPreferences.getInstance();

        // Make sure we don't have anything on disk.
        Assert.assertFalse(manager.contains(SEARCH_WIDGET_SEARCH_ENGINE_SHORTNAME));
        Assert.assertFalse(manager.contains(SEARCH_WIDGET_SEARCH_ENGINE_URL));
        Assert.assertFalse(manager.contains(SEARCH_WIDGET_IS_VOICE_SEARCH_AVAILABLE));
        Assert.assertFalse(manager.contains(SEARCH_WIDGET_IS_GOOGLE_LENS_AVAILABLE));
        Assert.assertFalse(manager.contains(SEARCH_WIDGET_IS_INCOGNITO_AVAILABLE));
        Assert.assertFalse(manager.contains(SEARCH_WIDGET_IS_AI_MODE_AVAILABLE));
        Assert.assertFalse(manager.contains(SEARCH_WIDGET_ACCOUNT_EMAIL));

        // Install receiver of the async pref update notification.
        // We expect the on-disk prefs to be already updated when this call is made.
        Consumer<SearchActivityPreferences> listener = mockPrefsConsumer();
        SearchActivityPreferencesManager.addObserver(listener);
        clearPrefsConsumerInvocations(listener);

        // Save settings to disk.
        SearchActivityPreferencesManager.setCurrentlyLoadedPreferences(mPreferences, true);
        // Should not be live right away - expect posted task.
        verify(listener, never()).accept(any());
        RobolectricUtil.runAllBackgroundAndUi();
        verify(listener).accept(eq(mPreferences));

        // Note: we provide different default values than stored ones to make sure everything works.
        Assert.assertEquals(
                "Search Engine",
                manager.readString(
                        SEARCH_WIDGET_SEARCH_ENGINE_SHORTNAME, "Engine Name Doesn't work"));

        GURL deserializedUrl =
                GURL.deserialize(manager.readString(SEARCH_WIDGET_SEARCH_ENGINE_URL, ""));
        Assert.assertEquals(mPreferences.searchEngineUrl, deserializedUrl);
        Assert.assertEquals(
                false, manager.readBoolean(SEARCH_WIDGET_IS_VOICE_SEARCH_AVAILABLE, true));
        Assert.assertEquals(
                true, manager.readBoolean(SEARCH_WIDGET_IS_GOOGLE_LENS_AVAILABLE, false));
        Assert.assertEquals(true, manager.readBoolean(SEARCH_WIDGET_IS_INCOGNITO_AVAILABLE, false));
        Assert.assertEquals(true, manager.readBoolean(SEARCH_WIDGET_IS_AI_MODE_AVAILABLE, false));
        Assert.assertEquals(
                "persisted@email.com", manager.readString(SEARCH_WIDGET_ACCOUNT_EMAIL, null));

        // Reset values to defaults / "clear application data". Make sure we don't have anything on
        // disk.
        SearchActivityPreferencesManager.resetCachedValues();
        Assert.assertFalse(manager.contains(SEARCH_WIDGET_SEARCH_ENGINE_SHORTNAME));
        Assert.assertFalse(manager.contains(SEARCH_WIDGET_SEARCH_ENGINE_URL));
        Assert.assertFalse(manager.contains(SEARCH_WIDGET_IS_VOICE_SEARCH_AVAILABLE));
        Assert.assertFalse(manager.contains(SEARCH_WIDGET_IS_GOOGLE_LENS_AVAILABLE));
        Assert.assertFalse(manager.contains(SEARCH_WIDGET_IS_INCOGNITO_AVAILABLE));
        Assert.assertFalse(manager.contains(SEARCH_WIDGET_IS_AI_MODE_AVAILABLE));
        Assert.assertFalse(manager.contains(SEARCH_WIDGET_ACCOUNT_EMAIL));
    }

    @Test
    public void managerTest_earlyInitializationOfTemplateUrlService() {
        // Install event listener.
        Consumer<SearchActivityPreferences> listener = mockPrefsConsumer();
        SearchActivityPreferencesManager.addObserver(listener);
        clearPrefsConsumerInvocations(listener);
        verifyNoMoreInteractions(mTemplateUrlServiceMock);

        // Signal the Manager that Native Libraries are ready.
        SearchActivityPreferencesManager.onNativeLibraryReady();
        verify(mTemplateUrlServiceMock, times(1)).registerLoadListener(any());
        verify(mTemplateUrlServiceMock, times(1)).addObserver(any());
        Assert.assertNotNull(mTemplateUrlServiceLoadListener);
        Assert.assertNotNull(mTemplateUrlServiceObserver);
        reset(mTemplateUrlServiceMock);

        // Confirm no crash if we don't have no DSE at the time of first call.
        // Confirm that we deregister load observer since it should no longer be needed.
        doReturn(true).when(mTemplateUrlServiceMock).isLoaded();
        mTemplateUrlServiceLoadListener.onTemplateUrlServiceLoaded();
        verify(mTemplateUrlServiceMock, times(1)).getDefaultSearchEngineTemplateUrl();
        verify(mTemplateUrlServiceMock, times(1))
                .unregisterLoadListener(eq(mTemplateUrlServiceLoadListener));

        // Confirm no data and no updates.
        Assert.assertNull(SearchActivityPreferencesManager.getCurrent().searchEngineName);
        Assert.assertTrue(SearchActivityPreferencesManager.getCurrent().searchEngineUrl.isEmpty());
        RobolectricUtil.runAllBackgroundAndUiIncludingDelayed();
        verify(listener, never()).accept(any());
    }

    @Test
    public void managerTest_lateInitializationOfTemplateUrlService() {
        // Install event listener.
        Consumer<SearchActivityPreferences> listener = mockPrefsConsumer();
        ArgumentCaptor<SearchActivityPreferences> refPrefs =
                ArgumentCaptor.forClass(SearchActivityPreferences.class);

        SearchActivityPreferencesManager.addObserver(listener);
        clearPrefsConsumerInvocations(listener);

        // Set up template url to have some data.
        doReturn("Cowabunga").when(mTemplateUrlMock).getShortName();
        doReturn("keyword").when(mTemplateUrlMock).getKeyword();
        doReturn("https://www.cowabunga.com/are-turtles-still-awesome?woooo")
                .when(mTemplateUrlServiceMock)
                .getSearchEngineUrlFromTemplateUrl(eq("keyword"));
        doReturn(mTemplateUrlMock)
                .when(mTemplateUrlServiceMock)
                .getDefaultSearchEngineTemplateUrl();

        // Signal the Manager that Native Libraries are ready.
        SearchActivityPreferencesManager.onNativeLibraryReady();

        // Simulate the event where we had everything readily available when TemplateUrlService is
        // loaded.
        doReturn(true).when(mTemplateUrlServiceMock).isLoaded();
        doReturn(mTemplateUrlMock)
                .when(mTemplateUrlServiceMock)
                .getDefaultSearchEngineTemplateUrl();
        mTemplateUrlServiceLoadListener.onTemplateUrlServiceLoaded();

        // Confirm data is available and update is pushed.
        RobolectricUtil.runAllBackgroundAndUi();
        verify(listener).accept(refPrefs.capture());
        Assert.assertEquals("Cowabunga", refPrefs.getValue().searchEngineName);
        Assert.assertEquals(
                "https://www.cowabunga.com/", refPrefs.getValue().searchEngineUrl.getSpec());
    }

    @Test
    public void initializeFromCache_withOldStyleUrl() {
        final SharedPreferencesManager manager = ChromeSharedPreferences.getInstance();

        manager.writeString(SEARCH_WIDGET_SEARCH_ENGINE_SHORTNAME, "Engine");
        manager.writeString(SEARCH_WIDGET_SEARCH_ENGINE_URL, "https://engine.com");

        // Force re-read persisted data.
        SearchActivityPreferencesManager.resetForTesting();
        SearchActivityPreferences data = SearchActivityPreferencesManager.getCurrent();

        Assert.assertEquals("Engine", data.searchEngineName);
        Assert.assertEquals("https://engine.com/", data.searchEngineUrl.getSpec());
    }

    @Test
    public void initializeFromCache_withSerializedUrl() {
        final SharedPreferencesManager manager = ChromeSharedPreferences.getInstance();

        manager.writeString(SEARCH_WIDGET_SEARCH_ENGINE_SHORTNAME, "Engine");
        manager.writeString(
                SEARCH_WIDGET_SEARCH_ENGINE_URL, new GURL("https://engine.com").serialize());

        // Force re-read persisted data.
        SearchActivityPreferencesManager.resetForTesting();
        SearchActivityPreferences data = SearchActivityPreferencesManager.getCurrent();

        Assert.assertEquals("Engine", data.searchEngineName);
        Assert.assertEquals("https://engine.com/", data.searchEngineUrl.getSpec());
    }

    @Test
    public void updateFeatureAvailability() {
        doReturn(true).when(mLensController).isLensEnabled(any());
        VoiceRecognitionUtil.setIsVoiceSearchEnabledForTesting(true);
        IncognitoUtils.setEnabledForTesting(true);
        ComposeplateUtils.setIsEnabledForTesting(true);

        SearchActivityPreferencesManager.updateFeatureAvailability(
                ContextUtils.getApplicationContext(), null);
        var data = SearchActivityPreferencesManager.getCurrent();
        Assert.assertTrue(data.googleLensAvailable);
        Assert.assertTrue(data.voiceSearchAvailable);
        Assert.assertTrue(data.incognitoAvailable);
        Assert.assertTrue(data.aiModeAvailable);
        Assert.assertNull(data.accountEmail);

        // Disable Lens.
        doReturn(false).when(mLensController).isLensEnabled(any());
        SearchActivityPreferencesManager.updateFeatureAvailability(
                ContextUtils.getApplicationContext(), null);
        data = SearchActivityPreferencesManager.getCurrent();
        Assert.assertFalse(data.googleLensAvailable);
        Assert.assertTrue(data.voiceSearchAvailable);
        Assert.assertTrue(data.incognitoAvailable);
        Assert.assertTrue(data.aiModeAvailable);
        Assert.assertNull(data.accountEmail);

        // Disable Voice.
        VoiceRecognitionUtil.setIsVoiceSearchEnabledForTesting(false);
        SearchActivityPreferencesManager.updateFeatureAvailability(
                ContextUtils.getApplicationContext(), null);
        data = SearchActivityPreferencesManager.getCurrent();
        Assert.assertFalse(data.googleLensAvailable);
        Assert.assertFalse(data.voiceSearchAvailable);
        Assert.assertTrue(data.incognitoAvailable);
        Assert.assertTrue(data.aiModeAvailable);
        Assert.assertNull(data.accountEmail);

        // Disable Incognito.
        IncognitoUtils.setEnabledForTesting(false);
        SearchActivityPreferencesManager.updateFeatureAvailability(
                ContextUtils.getApplicationContext(), null);
        data = SearchActivityPreferencesManager.getCurrent();
        Assert.assertFalse(data.googleLensAvailable);
        Assert.assertFalse(data.voiceSearchAvailable);
        Assert.assertFalse(data.incognitoAvailable);
        Assert.assertTrue(data.aiModeAvailable);
        Assert.assertNull(data.accountEmail);

        // Disable AI Mode.
        ComposeplateUtils.setIsEnabledForTesting(false);
        SearchActivityPreferencesManager.updateFeatureAvailability(
                ContextUtils.getApplicationContext(), null);
        data = SearchActivityPreferencesManager.getCurrent();
        Assert.assertFalse(data.googleLensAvailable);
        Assert.assertFalse(data.voiceSearchAvailable);
        Assert.assertFalse(data.incognitoAvailable);
        Assert.assertFalse(data.aiModeAvailable);
        Assert.assertNull(data.accountEmail);
    }

    @Test
    public void onTemplateUrlServiceChanged_retrieveNewEngineNameAndUrl() {
        var oldData = SearchActivityPreferencesManager.getCurrent();

        // Simulate change.
        doReturn("Engine").when(mTemplateUrlMock).getShortName();
        doReturn("keyword").when(mTemplateUrlMock).getKeyword();
        doReturn("https://www.engine.com/some/path?with=query")
                .when(mTemplateUrlServiceMock)
                .getSearchEngineUrlFromTemplateUrl(eq("keyword"));
        doReturn(mTemplateUrlMock)
                .when(mTemplateUrlServiceMock)
                .getDefaultSearchEngineTemplateUrl();
        SearchActivityPreferencesManager.get().onTemplateURLServiceChanged();

        var newData = SearchActivityPreferencesManager.getCurrent();
        Assert.assertNotEquals(oldData, newData);
        Assert.assertEquals("Engine", newData.searchEngineName);
        // We only expect origin: no path, no query.
        Assert.assertEquals("https://www.engine.com/", newData.searchEngineUrl.getSpec());
    }
}
