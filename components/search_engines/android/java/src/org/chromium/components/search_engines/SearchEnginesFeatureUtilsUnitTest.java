// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.search_engines;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;

import org.junit.After;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.CommandLine;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.components.search_engines.SearchEngineCountryDelegate.ChoiceScope;

@RunWith(BaseRobolectricTestRunner.class)
public class SearchEnginesFeatureUtilsUnitTest {

    @After
    public void tearDown() {
        CommandLine commandLine = CommandLine.getInstance();
        commandLine.removeSwitch(SearchEnginesFeatureUtils.ENABLE_CHOICE_APIS_DEBUG_SWITCH);
        commandLine.removeSwitch(SearchEnginesFeatureUtils.ENABLE_CHOICE_APIS_FAKE_BACKEND_SWITCH);
    }

    @Test
    public void testIsChoiceApisDebugEnabled() {
        assertFalse(SearchEnginesFeatureUtils.getInstance().isChoiceApisDebugEnabled());

        CommandLine.getInstance()
                .appendSwitch(SearchEnginesFeatureUtils.ENABLE_CHOICE_APIS_DEBUG_SWITCH);
        assertTrue(SearchEnginesFeatureUtils.getInstance().isChoiceApisDebugEnabled());
    }

    @Test
    public void testChoiceApisFakeBackendScope() {
        var featureUtils = SearchEnginesFeatureUtils.getInstance();
        assertNull(featureUtils.choiceApisFakeBackendScope());

        CommandLine.getInstance()
                .appendSwitch(SearchEnginesFeatureUtils.ENABLE_CHOICE_APIS_FAKE_BACKEND_SWITCH);
        assertEquals(
                Integer.valueOf(ChoiceScope.DEFAULT), featureUtils.choiceApisFakeBackendScope());

        CommandLine.getInstance()
                .appendSwitchWithValue(
                        SearchEnginesFeatureUtils.ENABLE_CHOICE_APIS_FAKE_BACKEND_SWITCH,
                        String.valueOf(ChoiceScope.DEFAULT));
        assertEquals(
                Integer.valueOf(ChoiceScope.DEFAULT), featureUtils.choiceApisFakeBackendScope());

        CommandLine.getInstance()
                .appendSwitchWithValue(
                        SearchEnginesFeatureUtils.ENABLE_CHOICE_APIS_FAKE_BACKEND_SWITCH,
                        String.valueOf(ChoiceScope.CHROME_SEARCH_ONLY));
        assertEquals(
                Integer.valueOf(ChoiceScope.CHROME_SEARCH_ONLY),
                featureUtils.choiceApisFakeBackendScope());

        CommandLine.getInstance()
                .appendSwitchWithValue(
                        SearchEnginesFeatureUtils.ENABLE_CHOICE_APIS_FAKE_BACKEND_SWITCH,
                        "invalid");
        assertEquals(
                Integer.valueOf(ChoiceScope.DEFAULT), featureUtils.choiceApisFakeBackendScope());
    }
}
