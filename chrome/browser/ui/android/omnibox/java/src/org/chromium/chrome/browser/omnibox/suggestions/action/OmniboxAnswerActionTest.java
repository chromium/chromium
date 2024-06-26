// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions.action;

import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;

/** Tests for {@link OmniboxAnswerAction}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class OmniboxAnswerActionTest {
    public @Rule MockitoRule mockitoRule = MockitoJUnit.rule();

    @Test
    public void executeDoesNothing() {
        OmniboxAnswerAction answerAction =
                (OmniboxAnswerAction)
                        OmniboxActionFactoryImpl.get()
                                .buildOmniboxAnswerAction(123L, "7 day forecast", "7 day forecast");
        answerAction.execute(null);
    }
}
