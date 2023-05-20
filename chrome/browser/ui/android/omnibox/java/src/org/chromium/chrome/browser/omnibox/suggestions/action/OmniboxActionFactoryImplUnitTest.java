// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions.action;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoMoreInteractions;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.JniMocker;
import org.chromium.components.omnibox.action.OmniboxActionFactoryJni;

/**
 * Tests for {@link OmniboxActionFactoryImpl}.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class OmniboxActionFactoryImplUnitTest {
    public @Rule MockitoRule mMockitoRule = MockitoJUnit.rule();
    public @Rule JniMocker mJniMocker = new JniMocker();
    private @Mock OmniboxActionFactoryJni mNatives;

    @Before
    public void setUp() {
        mJniMocker.mock(OmniboxActionFactoryJni.TEST_HOOKS, mNatives);
    }

    @After
    public void tearDown() {
        verifyNoMoreInteractions(mNatives);
    }

    @Test
    public void factoryIsCreatedOnlyOnce() {
        var factory1 = OmniboxActionFactoryImpl.get();
        var factory2 = OmniboxActionFactoryImpl.get();

        assertEquals(factory1, factory2);
    }

    @Test
    public void nativeInitializationPassesInstance() {
        var factory = OmniboxActionFactoryImpl.get();

        factory.initNativeFactory();
        verify(mNatives, times(1)).setFactory(factory);
    }

    @Test
    public void nativeDestructionClearsInstance() {
        var factory = OmniboxActionFactoryImpl.get();

        factory.destroyNativeFactory();
        verify(mNatives, times(1)).setFactory(null);
    }

    @Test
    public void historyClustersDowncasting() {
        // The underlying code will throw if instance is not valid.
        // Checking for null in case that changes.
        assertNotNull(HistoryClustersAction.from(
                OmniboxActionFactoryImpl.get().buildHistoryClustersAction("hint", "query")));
    }

    @Test
    public void omniboxPedalsDowncasting() {
        // The underlying code will throw if instance is not valid.
        // Checking for null in case that changes.
        assertNotNull(
                OmniboxPedal.from(OmniboxActionFactoryImpl.get().buildOmniboxPedal("hint", 1)));
    }

    @Test
    public void omniboxActionInSuggestDowncasting() {
        // The underlying code will throw if instance is not valid.
        // Checking for null in case that changes.
        assertNotNull(OmniboxActionInSuggest.from(
                OmniboxActionFactoryImpl.get().buildActionInSuggest("hint", 1, "url")));
    }
}
