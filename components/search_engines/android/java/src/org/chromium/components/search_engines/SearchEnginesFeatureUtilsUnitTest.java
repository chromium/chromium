// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.search_engines;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.CommandLine;
import org.chromium.base.test.BaseRobolectricTestRunner;

@SmallTest
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
    public void testIsChoiceApisFakeBackendEnabled() {
        assertFalse(SearchEnginesFeatureUtils.getInstance().isChoiceApisFakeBackendEnabled());

        CommandLine.getInstance()
                .appendSwitch(SearchEnginesFeatureUtils.ENABLE_CHOICE_APIS_FAKE_BACKEND_SWITCH);
        assertTrue(SearchEnginesFeatureUtils.getInstance().isChoiceApisFakeBackendEnabled());
    }
}
