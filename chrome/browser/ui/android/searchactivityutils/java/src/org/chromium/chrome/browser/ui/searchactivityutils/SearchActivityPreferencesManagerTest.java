// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.searchactivityutils;

import static org.mockito.Mockito.anyObject;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.eq;
import static org.mockito.Mockito.reset;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoMoreInteractions;

import static org.chromium.chrome.browser.preferences.ChromePreferenceKeys.SEARCH_WIDGET_IS_GOOGLE_LENS_AVAILABLE;
import static org.chromium.chrome.browser.preferences.ChromePreferenceKeys.SEARCH_WIDGET_IS_INCOGNITO_AVAILABLE;
import static org.chromium.chrome.browser.preferences.ChromePreferenceKeys.SEARCH_WIDGET_IS_VOICE_SEARCH_AVAILABLE;
import static org.chromium.chrome.browser.preferences.ChromePreferenceKeys.SEARCH_WIDGET_SEARCH_ENGINE_SHORTNAME;
import static org.chromium.chrome.browser.preferences.ChromePreferenceKeys.SEARCH_WIDGET_SEARCH_ENGINE_URL;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import org.chromium.base.library_loader.LibraryLoader;
import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.UiThreadTest;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.chrome.browser.preferences.SharedPreferencesManager;
import org.chromium.chrome.browser.search_engines.TemplateUrlServiceFactory;
import org.chromium.chrome.browser.ui.searchactivityutils.SearchActivityPreferencesManager.SearchActivityPreferences;
import org.chromium.components.search_engines.TemplateUrl;
import org.chromium.components.search_engines.TemplateUrlService;
import org.chromium.components.search_engines.TemplateUrlService.LoadListener;
import org.chromium.components.search_engines.TemplateUrlService.TemplateUrlServiceObserver;
import org.chromium.content_public.browser.test.NativeLibraryTestUtils;
import org.chromium.content_public.browser.test.util.TestThreadUtils;

import java.util.concurrent.atomic.AtomicInteger;
import java.util.concurrent.atomic.AtomicReference;
import java.util.function.Consumer;

/**
 * Tests for {@link SearchActivityPreferencesManager}.
 */
@RunWith(BaseJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
public class SearchActivityPreferencesManagerTest {
    @Mock
    private TemplateUrlService mTemplateUrlServiceMock;

    @Mock
    private LibraryLoader mLibraryLoaderMock;

    @Mock
    private TemplateUrl mTemplateUrlMock;

    private LoadListener mTemplateUrlServiceLoadListener;
    private TemplateUrlServiceObserver mTemplateUrlServiceObserver;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        NativeLibraryTestUtils.loadNativeLibraryNoBrowserProcess();
        TemplateUrlServiceFactory.setInstanceForTesting(mTemplateUrlServiceMock);
        LibraryLoader.setLibraryLoaderForTesting(mLibraryLoaderMock);

        doAnswer(invocation -> {
            mTemplateUrlServiceLoadListener = (LoadListener) invocation.getArguments()[0];
            return null;
        })
                .when(mTemplateUrlServiceMock)
                .registerLoadListener(anyObject());

        doAnswer(invocation -> {
            mTemplateUrlServiceObserver = (TemplateUrlServiceObserver) invocation.getArguments()[0];
            return null;
        })
                .when(mTemplateUrlServiceMock)
                .addObserver(anyObject());

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            SearchActivityPreferencesManager.resetForTesting();
            // Reseta any cached values so we consistently start with a predictable state.
            SearchActivityPreferencesManager.resetCachedValues();
        });

        // Make sure there were no premature attempts to register observers.
        Assert.assertNull(mTemplateUrlServiceLoadListener);
        Assert.assertNull(mTemplateUrlServiceObserver);
    }

    @Test
    @SmallTest
    @UiThreadTest
    public void preferenceTest_equalWithSameContent() {
        SearchActivityPreferences p1 =
                new SearchActivityPreferences("test", "test.url", true, true, true);
        SearchActivityPreferences p2 =
                new SearchActivityPreferences("test", "test.url", true, true, true);
        Assert.assertEquals(p1, p2);
        Assert.assertEquals(p1.hashCode(), p2.hashCode());

        p1 = new SearchActivityPreferences(null, "test.url", true, false, true);
        p2 = new SearchActivityPreferences(null, "test.url", true, false, true);
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
    @SmallTest
    @UiThreadTest
    public void preferenceTest_notEqualWithDifferentVoiceAvailability() {
        SearchActivityPreferences p1 =
                new SearchActivityPreferences("test", "test.url", true, false, false);
        SearchActivityPreferences p2 =
                new SearchActivityPreferences("test", "test.url", false, false, false);
        Assert.assertNotEquals(p1, p2);
        Assert.assertNotEquals(p1.hashCode(), p2.hashCode());
    }

    @Test
    @SmallTest
    @UiThreadTest
    public void preferenceTest_notEqualWithDifferentLensAvailability() {
        SearchActivityPreferences p1 =
                new SearchActivityPreferences("test", "test.url", true, true, false);
        SearchActivityPreferences p2 =
                new SearchActivityPreferences("test", "test.url", true, false, false);
        Assert.assertNotEquals(p1, p2);
        Assert.assertNotEquals(p1.hashCode(), p2.hashCode());
    }

    @Test
    @SmallTest
    @UiThreadTest
    public void preferenceTest_notEqualWithDifferentIncognitoAvailability() {
        SearchActivityPreferences p1 =
                new SearchActivityPreferences("test", "test.url", true, true, true);
        SearchActivityPreferences p2 =
                new SearchActivityPreferences("test", "test.url", true, true, false);
        Assert.assertNotEquals(p1, p2);
        Assert.assertNotEquals(p1.hashCode(), p2.hashCode());
    }

    @Test
    @SmallTest
    @UiThreadTest
    public void preferenceTest_notEqualWithDifferentSearchEngineName() {
        SearchActivityPreferences p1 =
                new SearchActivityPreferences("Search Engine 1", "test.url", true, true, true);
        SearchActivityPreferences p2 =
                new SearchActivityPreferences("Search Engine 2", "test.url", true, true, true);
        Assert.assertNotEquals(p1, p2);
        Assert.assertNotEquals(p1.hashCode(), p2.hashCode());
    }

    @Test
    @SmallTest
    @UiThreadTest
    public void preferenceTest_notEqualWithDifferentSearchEngineUrl() {
        SearchActivityPreferences p1 =
                new SearchActivityPreferences("Google", "www.google.com", true, true, true);
        SearchActivityPreferences p2 =
                new SearchActivityPreferences("Google", "www.google.pl", true, true, true);
        Assert.assertNotEquals(p1, p2);
        Assert.assertNotEquals(p1.hashCode(), p2.hashCode());
    }

    @Test
    @SmallTest
    @UiThreadTest
    public void managerTest_updateIsPropagatedToAllObservers() {
        final AtomicInteger numCalls = new AtomicInteger(0);
        // Add 2 distinct listeners and confirm everybody gets called immediately with initial
        // values.
        SearchActivityPreferencesManager.addObserver(prefs -> numCalls.incrementAndGet());
        CriteriaHelper.pollUiThreadNested(() -> numCalls.get() == 1);
        SearchActivityPreferencesManager.addObserver(prefs -> numCalls.incrementAndGet());
        CriteriaHelper.pollUiThreadNested(() -> numCalls.get() == 2);

        // Perform an update and check the number of calls.
        numCalls.set(0);
        SearchActivityPreferencesManager.setCurrentlyLoadedPreferences(
                new SearchActivityPreferences("Search Engine", "URL", false, true, true), false);
        CriteriaHelper.pollUiThreadNested(() -> numCalls.get() == 2);

        // Add a new listener.
        numCalls.set(0);
        SearchActivityPreferencesManager.addObserver(prefs -> numCalls.incrementAndGet());
        CriteriaHelper.pollUiThreadNested(() -> numCalls.get() == 1);

        // Perform an update and check the number of calls.
        numCalls.set(0);
        SearchActivityPreferencesManager.setCurrentlyLoadedPreferences(
                new SearchActivityPreferences("Search Engine", "URL", true, true, true), false);
        CriteriaHelper.pollUiThreadNested(() -> numCalls.get() == 3);

        // Finally, reset settings to safe defaults. All listeners should be notified.
        numCalls.set(0);
        SearchActivityPreferencesManager.resetCachedValues();
        CriteriaHelper.pollUiThreadNested(() -> numCalls.get() == 3);
    }

    @Test
    @SmallTest
    @UiThreadTest
    public void managerTest_eachObserverCanOnlyBeAddedOnce() {
        final AtomicInteger numCalls = new AtomicInteger(0);
        final Consumer<SearchActivityPreferences> listener = prefs -> numCalls.incrementAndGet();

        // Add same listener a few times.
        SearchActivityPreferencesManager.addObserver(listener);
        Assert.assertEquals(1, numCalls.get());
        SearchActivityPreferencesManager.addObserver(listener);
        Assert.assertEquals(1, numCalls.get());

        // Add a different listener.
        SearchActivityPreferencesManager.addObserver(prefs -> numCalls.incrementAndGet());
        Assert.assertEquals(2, numCalls.get());
        SearchActivityPreferencesManager.addObserver(listener);
        Assert.assertEquals(2, numCalls.get());

        // Verify that we don't get excessive update notifications.
        numCalls.set(0);
        SearchActivityPreferencesManager.setCurrentlyLoadedPreferences(
                new SearchActivityPreferences("ABC", "abc.xyz", false, true, true), false);
        CriteriaHelper.pollUiThreadNested(() -> numCalls.get() == 2);

        // Finally, confirm reset.
        numCalls.set(0);
        SearchActivityPreferencesManager.resetCachedValues();
        CriteriaHelper.pollUiThreadNested(() -> numCalls.get() == 2);
    }

    @Test
    @SmallTest
    @UiThreadTest
    public void managerTest_preferencesRetentionTest() {
        final SharedPreferencesManager manager = SharedPreferencesManager.getInstance();

        // Make sure we don't have anything on disk.
        Assert.assertFalse(manager.contains(SEARCH_WIDGET_SEARCH_ENGINE_SHORTNAME));
        Assert.assertFalse(manager.contains(SEARCH_WIDGET_SEARCH_ENGINE_URL));
        Assert.assertFalse(manager.contains(SEARCH_WIDGET_IS_VOICE_SEARCH_AVAILABLE));
        Assert.assertFalse(manager.contains(SEARCH_WIDGET_IS_GOOGLE_LENS_AVAILABLE));
        Assert.assertFalse(manager.contains(SEARCH_WIDGET_IS_INCOGNITO_AVAILABLE));

        // Install receiver of the async pref update notification.
        // We expect the on-disk prefs to be already updated when this call is made.
        final AtomicInteger numCalls = new AtomicInteger(0);
        final Consumer<SearchActivityPreferences> listener = prefs -> numCalls.incrementAndGet();
        SearchActivityPreferencesManager.addObserver(listener);
        numCalls.set(0);

        // Save settings to disk.
        SearchActivityPreferencesManager.setCurrentlyLoadedPreferences(
                new SearchActivityPreferences("Search Engine", "URL", false, true, true), true);
        CriteriaHelper.pollUiThreadNested(() -> numCalls.get() == 1);

        // Note: we provide different default values than stored ones to make sure everything works.
        Assert.assertEquals("Search Engine",
                manager.readString(
                        SEARCH_WIDGET_SEARCH_ENGINE_SHORTNAME, "Engine Name Doesn't work"));
        Assert.assertEquals("URL",
                manager.readString(SEARCH_WIDGET_SEARCH_ENGINE_URL, "Engine URL Doesn't work"));
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
    @SmallTest
    @UiThreadTest
    public void managerTest_earlyInitializationOfTemplateUrlService() {
        // Install event listener.
        final AtomicInteger numCalls = new AtomicInteger(0);
        final Consumer<SearchActivityPreferences> listener = prefs -> numCalls.incrementAndGet();
        SearchActivityPreferencesManager.addObserver(listener);
        numCalls.set(0);
        verifyNoMoreInteractions(mTemplateUrlServiceMock);

        // Signal the Manager that Native Libraries are ready.
        doReturn(true).when(mLibraryLoaderMock).isInitialized();
        SearchActivityPreferencesManager.onNativeLibraryReady();
        verify(mTemplateUrlServiceMock, times(1)).registerLoadListener(anyObject());
        verify(mTemplateUrlServiceMock, times(1)).addObserver(anyObject());
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
        Assert.assertNull(SearchActivityPreferencesManager.getCurrent().searchEngineUrl);
        CriteriaHelper.pollUiThreadNested(() -> numCalls.get() == 0);
    }

    @Test
    @SmallTest
    @UiThreadTest
    public void managerTest_lateInitializationOfTemplateUrlService() {
        // Install event listener.
        final AtomicInteger numCalls = new AtomicInteger(0);
        final AtomicReference<SearchActivityPreferences> refPrefs = new AtomicReference<>();
        final Consumer<SearchActivityPreferences> listener = prefs -> {
            numCalls.incrementAndGet();
            refPrefs.set(prefs);
        };
        SearchActivityPreferencesManager.addObserver(listener);
        numCalls.set(0);

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
        CriteriaHelper.pollUiThreadNested(() -> numCalls.get() == 1);
        Assert.assertEquals("Cowabunga", refPrefs.get().searchEngineName);
        Assert.assertEquals("https://www.cowabunga.com/", refPrefs.get().searchEngineUrl);
    }
}
