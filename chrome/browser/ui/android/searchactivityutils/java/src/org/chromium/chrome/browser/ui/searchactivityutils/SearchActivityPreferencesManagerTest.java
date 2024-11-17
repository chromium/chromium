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

import static org.chromium.chrome.browser.preferences.ChromePreferenceKeys.SEARCH_WIDGET_IS_GOOGLE_LENS_AVAILABLE;
import static org.chromium.chrome.browser.preferences.ChromePreferenceKeys.SEARCH_WIDGET_IS_INCOGNITO_AVAILABLE;
import static org.chromium.chrome.browser.preferences.ChromePreferenceKeys.SEARCH_WIDGET_IS_VOICE_SEARCH_AVAILABLE;
import static org.chromium.chrome.browser.preferences.ChromePreferenceKeys.SEARCH_WIDGET_SEARCH_ENGINE_SHORTNAME;
import static org.chromium.chrome.browser.preferences.ChromePreferenceKeys.SEARCH_WIDGET_SEARCH_ENGINE_URL;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;
import org.robolectric.annotation.Implements;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.ContextUtils;
import org.chromium.base.library_loader.LibraryLoader;
import org.chromium.base.shared_preferences.SharedPreferencesManager;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.incognito.IncognitoUtils;
import org.chromium.chrome.browser.lens.LensController;
import org.chromium.chrome.browser.omnibox.voice.VoiceRecognitionUtil;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.chrome.browser.search_engines.TemplateUrlServiceFactory;
import org.chromium.chrome.browser.ui.searchactivityutils.SearchActivityPreferencesManager.SearchActivityPreferences;
import org.chromium.components.search_engines.TemplateUrl;
import org.chromium.components.search_engines.TemplateUrlService;
import org.chromium.components.search_engines.TemplateUrlService.LoadListener;
import org.chromium.components.search_engines.TemplateUrlService.TemplateUrlServiceObserver;
import org.chromium.ui.permissions.AndroidPermissionDelegate;
import org.chromium.url.GURL;

import java.util.function.Consumer;

/** Tests for {@link SearchActivityPreferencesManager}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(
        shadows = {
            SearchActivityPreferencesManagerTest.ShadowLensController.class,
            SearchActivityPreferencesManagerTest.ShadowVoiceRecognitionUtil.class,
        })
public class SearchActivityPreferencesManagerTest {
    @Mock private TemplateUrlService mTemplateUrlServiceMock;
    @Mock private LibraryLoader mLibraryLoaderMock;
    @Mock private TemplateUrl mTemplateUrlMock;
    @Mock private Profile mProfile;

    private LoadListener mTemplateUrlServiceLoadListener;
    private TemplateUrlServiceObserver mTemplateUrlServiceObserver;

    @Implements(LensController.class)
    public static class ShadowLensController {
        public static boolean sIsAvailable = true;

        public static LensController getInstance() {
            var controller = mock(LensController.class);
            doAnswer(i -> sIsAvailable).when(controller).isLensEnabled(any());
            return controller;
        }
    }

    @Implements(VoiceRecognitionUtil.class)
    public static class ShadowVoiceRecognitionUtil {
        public static boolean sIsAvailable = true;

        public static boolean isVoiceSearchEnabled(AndroidPermissionDelegate delegate) {
            return sIsAvailable;
        }
    }

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        TemplateUrlServiceFactory.setInstanceForTesting(mTemplateUrlServiceMock);
        ProfileManager.setLastUsedProfileForTesting(mProfile);

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
        ShadowLooper.runUiThreadTasks();
    }

    @After
    public void tearDown() {
        ShadowLooper.runUiThreadTasks();
        TemplateUrlServiceFactory.setInstanceForTesting(null);
        ProfileManager.setLastUsedProfileForTesting(null);
        SearchActivityPreferencesManager.resetForTesting();
    }

    @Test
    public void preferenceTest_equalWithSameContent() {
        SearchActivityPreferences p1 =
                new SearchActivityPreferences(
                        "test", new GURL("https://test.url"), true, true, true);
        SearchActivityPreferences p2 =
                new SearchActivityPreferences(
                        "test", new GURL("https://test.url"), true, true, true);
        Assert.assertEquals(p1, p2);
        Assert.assertEquals(p1.hashCode(), p2.hashCode());

        p1 = new SearchActivityPreferences(null, new GURL("https://test.url"), true, false, true);
        p2 = new SearchActivityPreferences(null, new GURL("https://test.url"), true, false, true);
        Assert.assertEquals(p1, p2);
        Assert.assertEquals(p1.hashCode(), p2.hashCode());

        p1 = new SearchActivityPreferences("test", null, false, true, true);
        p2 = new SearchActivityPreferences("test", null, false, true, true);
        Assert.assertEquals(p1, p2);
        Assert.assertEquals(p1.hashCode(), p2.hashCode());

        p1 = new SearchActivityPreferences(null, null, false, false, false);
        p2 = new SearchActivityPreferences(null, null, false, false, false);
        Assert.assertEquals(p1, p2);
        Assert.assertEquals(p1.hashCode(), p2.hashCode());
    }

    @Test
    public void preferenceTest_notEqualWithDifferentVoiceAvailability() {
        SearchActivityPreferences p1 =
                new SearchActivityPreferences(
                        "test", new GURL("https://test.url"), true, false, false);
        SearchActivityPreferences p2 =
                new SearchActivityPreferences(
                        "test", new GURL("https://test.url"), false, false, false);
        Assert.assertNotEquals(p1, p2);
        Assert.assertNotEquals(p1.hashCode(), p2.hashCode());
    }

    @Test
    public void preferenceTest_notEqualWithDifferentLensAvailability() {
        SearchActivityPreferences p1 =
                new SearchActivityPreferences(
                        "test", new GURL("https://test.url"), true, true, false);
        SearchActivityPreferences p2 =
                new SearchActivityPreferences(
                        "test", new GURL("https://test.url"), true, false, false);
        Assert.assertNotEquals(p1, p2);
        Assert.assertNotEquals(p1.hashCode(), p2.hashCode());
    }

    @Test
    public void preferenceTest_notEqualWithDifferentIncognitoAvailability() {
        SearchActivityPreferences p1 =
                new SearchActivityPreferences(
                        "test", new GURL("https://test.url"), true, true, true);
        SearchActivityPreferences p2 =
                new SearchActivityPreferences(
                        "test", new GURL("https://test.url"), true, true, false);
        Assert.assertNotEquals(p1, p2);
        Assert.assertNotEquals(p1.hashCode(), p2.hashCode());
    }

    @Test
    public void preferenceTest_notEqualWithDifferentSearchEngineName() {
        SearchActivityPreferences p1 =
                new SearchActivityPreferences(
                        "Search Engine 1", new GURL("https://test.url"), true, true, true);
        SearchActivityPreferences p2 =
                new SearchActivityPreferences(
                        "Search Engine 2", new GURL("https://test.url"), true, true, true);
        Assert.assertNotEquals(p1, p2);
        Assert.assertNotEquals(p1.hashCode(), p2.hashCode());
    }

    @Test
    public void preferenceTest_notEqualWithDifferentSearchEngineUrl() {
        SearchActivityPreferences p1 =
                new SearchActivityPreferences(
                        "Google", new GURL("https://www.google.com"), true, true, true);
        SearchActivityPreferences p2 =
                new SearchActivityPreferences(
                        "Google", new GURL("https://www.google.pl"), true, true, true);
        Assert.assertNotEquals(p1, p2);
        Assert.assertNotEquals(p1.hashCode(), p2.hashCode());
    }

    @Test
    public void managerTest_updateIsPropagatedToAllObservers() {
        Consumer<SearchActivityPreferences> observer1 = mock(Consumer.class);
        Consumer<SearchActivityPreferences> observer2 = mock(Consumer.class);

        // Add 2 distinct listeners and confirm everybody gets called immediately with initial
        // values.
        SearchActivityPreferencesManager.addObserver(observer1);
        verify(observer1).accept(any());
        SearchActivityPreferencesManager.addObserver(observer2);
        verify(observer1).accept(any());
        clearInvocations(observer1, observer2);

        // Perform an update and check the number of calls.
        var newSettings =
                new SearchActivityPreferences(
                        "Search Engine", new GURL("https://URL"), false, true, true);
        SearchActivityPreferencesManager.setCurrentlyLoadedPreferences(newSettings, false);
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
        verify(observer1).accept(eq(newSettings));
        verify(observer2).accept(eq(newSettings));
        clearInvocations(observer1, observer2);

        // Add a new listener.
        Consumer<SearchActivityPreferences> observer3 = mock(Consumer.class);
        SearchActivityPreferencesManager.addObserver(observer3);
        verify(observer3).accept(eq(newSettings));
        clearInvocations(observer1, observer2, observer3);

        // Perform an update and check the number of calls.
        newSettings =
                new SearchActivityPreferences(
                        "Search Engine", new GURL("https://URL"), true, true, true);
        SearchActivityPreferencesManager.setCurrentlyLoadedPreferences(newSettings, false);
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
        verify(observer1).accept(eq(newSettings));
        verify(observer2).accept(eq(newSettings));
        verify(observer3).accept(eq(newSettings));
        clearInvocations(observer1, observer2, observer3);

        // Finally, reset settings to safe defaults. All listeners should be notified.
        SearchActivityPreferencesManager.resetCachedValues();
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
        verify(observer1).accept(any());
        verify(observer2).accept(any());
        verify(observer3).accept(any());
    }

    @Test
    public void managerTest_eachObserverCanOnlyBeAddedOnce() {
        final Consumer<SearchActivityPreferences> listener1 = mock(Consumer.class);

        // Add same listener a few times.
        SearchActivityPreferencesManager.addObserver(listener1);
        verify(listener1).accept(any());
        clearInvocations(listener1);

        SearchActivityPreferencesManager.addObserver(listener1);
        verify(listener1, never()).accept(any());

        // Add a different listener.
        Consumer<SearchActivityPreferences> listener2 = mock(Consumer.class);
        SearchActivityPreferencesManager.addObserver(listener2);
        verify(listener1, never()).accept(any());
        verify(listener2).accept(any());
        clearInvocations(listener1, listener2);

        SearchActivityPreferencesManager.addObserver(listener2);
        SearchActivityPreferencesManager.addObserver(listener1);
        verify(listener1, never()).accept(any());
        verify(listener2, never()).accept(any());

        // Verify that we don't get excessive update notifications.
        SearchActivityPreferencesManager.setCurrentlyLoadedPreferences(
                new SearchActivityPreferences(
                        "ABC", new GURL("https://abc.xyz"), false, true, true),
                false);
        verify(listener1, never()).accept(any());
        verify(listener2, never()).accept(any());
        ShadowLooper.runUiThreadTasks();
        verify(listener1).accept(any());
        verify(listener2).accept(any());
        clearInvocations(listener1, listener2);

        // Finally, confirm reset.
        SearchActivityPreferencesManager.resetCachedValues();
        verify(listener1, never()).accept(any());
        verify(listener2, never()).accept(any());
        ShadowLooper.runUiThreadTasks();
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

        // Install receiver of the async pref update notification.
        // We expect the on-disk prefs to be already updated when this call is made.
        Consumer<SearchActivityPreferences> listener = mock(Consumer.class);
        SearchActivityPreferencesManager.addObserver(listener);
        clearInvocations(listener);

        // Save settings to disk.
        var persistedUrl = new GURL("https://URL");
        var preference =
                new SearchActivityPreferences("Search Engine", persistedUrl, false, true, true);
        SearchActivityPreferencesManager.setCurrentlyLoadedPreferences(preference, true);
        // Should not be live right away - expect posted task.
        verify(listener, never()).accept(any());
        ShadowLooper.runUiThreadTasks();
        verify(listener).accept(eq(preference));

        // Note: we provide different default values than stored ones to make sure everything works.
        Assert.assertEquals(
                "Search Engine",
                manager.readString(
                        SEARCH_WIDGET_SEARCH_ENGINE_SHORTNAME, "Engine Name Doesn't work"));

        GURL deserializedUrl =
                GURL.deserialize(manager.readString(SEARCH_WIDGET_SEARCH_ENGINE_URL, ""));
        Assert.assertEquals(persistedUrl, deserializedUrl);
        Assert.assertEquals(
                false, manager.readBoolean(SEARCH_WIDGET_IS_VOICE_SEARCH_AVAILABLE, true));
        Assert.assertEquals(
                true, manager.readBoolean(SEARCH_WIDGET_IS_GOOGLE_LENS_AVAILABLE, false));
        Assert.assertEquals(true, manager.readBoolean(SEARCH_WIDGET_IS_INCOGNITO_AVAILABLE, false));

        // Reset values to defaults / "clear application data". Make sure we don't have anything on
        // disk.
        SearchActivityPreferencesManager.resetCachedValues();
        Assert.assertFalse(manager.contains(SEARCH_WIDGET_SEARCH_ENGINE_SHORTNAME));
        Assert.assertFalse(manager.contains(SEARCH_WIDGET_SEARCH_ENGINE_URL));
        Assert.assertFalse(manager.contains(SEARCH_WIDGET_IS_VOICE_SEARCH_AVAILABLE));
        Assert.assertFalse(manager.contains(SEARCH_WIDGET_IS_GOOGLE_LENS_AVAILABLE));
        Assert.assertFalse(manager.contains(SEARCH_WIDGET_IS_INCOGNITO_AVAILABLE));
    }

    @Test
    public void managerTest_earlyInitializationOfTemplateUrlService() {
        // Install event listener.
        Consumer<SearchActivityPreferences> listener = mock(Consumer.class);
        SearchActivityPreferencesManager.addObserver(listener);
        clearInvocations(listener);
        verifyNoMoreInteractions(mTemplateUrlServiceMock);

        // Signal the Manager that Native Libraries are ready.
        doReturn(true).when(mLibraryLoaderMock).isInitialized();
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
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
        verify(listener, never()).accept(any());
    }

    @Test
    public void managerTest_lateInitializationOfTemplateUrlService() {
        // Install event listener.
        Consumer<SearchActivityPreferences> listener = mock(Consumer.class);
        ArgumentCaptor<SearchActivityPreferences> refPrefs =
                ArgumentCaptor.forClass(SearchActivityPreferences.class);

        SearchActivityPreferencesManager.addObserver(listener);
        clearInvocations(listener);

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
        doReturn(true).when(mLibraryLoaderMock).isInitialized();
        SearchActivityPreferencesManager.onNativeLibraryReady();

        // Simulate the event where we had everything readily available when TemplateUrlService is
        // loaded.
        doReturn(true).when(mTemplateUrlServiceMock).isLoaded();
        doReturn(mTemplateUrlMock)
                .when(mTemplateUrlServiceMock)
                .getDefaultSearchEngineTemplateUrl();
        mTemplateUrlServiceLoadListener.onTemplateUrlServiceLoaded();

        // Confirm data is available and update is pushed.
        ShadowLooper.runUiThreadTasks();
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
        ShadowLensController.sIsAvailable = true;
        ShadowVoiceRecognitionUtil.sIsAvailable = true;
        IncognitoUtils.setEnabledForTesting(true);

        SearchActivityPreferencesManager.updateFeatureAvailability(
                ContextUtils.getApplicationContext(), null);
        var data = SearchActivityPreferencesManager.getCurrent();
        Assert.assertTrue(data.googleLensAvailable);
        Assert.assertTrue(data.voiceSearchAvailable);
        Assert.assertTrue(data.incognitoAvailable);

        // Disable Lens.
        ShadowLensController.sIsAvailable = false;
        SearchActivityPreferencesManager.updateFeatureAvailability(
                ContextUtils.getApplicationContext(), null);
        data = SearchActivityPreferencesManager.getCurrent();
        Assert.assertFalse(data.googleLensAvailable);
        Assert.assertTrue(data.voiceSearchAvailable);
        Assert.assertTrue(data.incognitoAvailable);

        // Disable Voice.
        ShadowVoiceRecognitionUtil.sIsAvailable = false;
        SearchActivityPreferencesManager.updateFeatureAvailability(
                ContextUtils.getApplicationContext(), null);
        data = SearchActivityPreferencesManager.getCurrent();
        Assert.assertFalse(data.googleLensAvailable);
        Assert.assertFalse(data.voiceSearchAvailable);
        Assert.assertTrue(data.incognitoAvailable);

        // Disable Incognito.
        IncognitoUtils.setEnabledForTesting(false);
        SearchActivityPreferencesManager.updateFeatureAvailability(
                ContextUtils.getApplicationContext(), null);
        data = SearchActivityPreferencesManager.getCurrent();
        Assert.assertFalse(data.googleLensAvailable);
        Assert.assertFalse(data.voiceSearchAvailable);
        Assert.assertFalse(data.incognitoAvailable);
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
