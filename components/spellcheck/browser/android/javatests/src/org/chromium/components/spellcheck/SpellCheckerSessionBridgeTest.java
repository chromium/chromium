// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.spellcheck;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.Mockito.doReturn;

import android.content.Context;
import android.view.textservice.SentenceSuggestionsInfo;
import android.view.textservice.SpellCheckerSession;
import android.view.textservice.SuggestionsInfo;
import android.view.textservice.TextServicesManager;

import androidx.test.core.app.ApplicationProvider;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;
import org.robolectric.shadow.api.Shadow;
import org.robolectric.shadows.ShadowApplication;

import org.chromium.base.ContextUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.HistogramWatcher;

/** Robolectric unit tests for {@link SpellCheckerSessionBridge}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class SpellCheckerSessionBridgeTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private SpellCheckerSessionBridge.Natives mNativeMock;
    @Mock private TextServicesManager mTextServicesManagerMock;
    @Mock private SpellCheckerSession mSpellCheckerSessionMock;

    @Before
    public void setUp() {
        ContextUtils.initApplicationContextForTests(ApplicationProvider.getApplicationContext());
        SpellCheckerSessionBridgeJni.setInstanceForTesting(mNativeMock);

        ShadowApplication shadowApplication =
                Shadow.extract(ApplicationProvider.getApplicationContext());
        shadowApplication.setSystemService(
                Context.TEXT_SERVICES_MANAGER_SERVICE, mTextServicesManagerMock);
        doReturn(mSpellCheckerSessionMock)
                .when(mTextServicesManagerMock)
                .newSpellCheckerSession(any(), any(), any(), anyBoolean());
    }

    @Test
    public void testRequestTextCheckHistogram() {
        var watcher =
                HistogramWatcher.newSingleRecordWatcher(
                        SpellCheckerSessionBridge.HISTOGRAM_REQUEST, true);
        SpellCheckerSessionBridge bridge = SpellCheckerSessionBridge.create(1234L, true, true);
        bridge.requestTextCheck("hello world", new SpellingMarker[0]);
        watcher.assertExpected();
    }

    @Test
    public void testOnGetSentenceSuggestionsHistograms() {
        var watcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                SpellCheckerSessionBridge.HISTOGRAM_UNDERLINE_LENGTH_SPELLING, 5)
                        .expectIntRecord(
                                SpellCheckerSessionBridge.HISTOGRAM_UNDERLINE_LENGTH_GRAMMAR, 8)
                        .expectBooleanRecord(
                                SpellCheckerSessionBridge.HISTOGRAM_COUNT_SPELLING, true)
                        .expectBooleanRecord(
                                SpellCheckerSessionBridge.HISTOGRAM_COUNT_GRAMMAR, true)
                        .build();

        SuggestionsInfo spellingInfo =
                new SuggestionsInfo(
                        SuggestionsInfo.RESULT_ATTR_LOOKS_LIKE_TYPO, new String[] {"test"});
        SuggestionsInfo grammarInfo =
                new SuggestionsInfo(
                        SuggestionsInfo.RESULT_ATTR_LOOKS_LIKE_GRAMMAR_ERROR,
                        new String[] {"test"});

        SentenceSuggestionsInfo info1 =
                new SentenceSuggestionsInfo(
                        new SuggestionsInfo[] {spellingInfo}, new int[] {0}, new int[] {5});
        SentenceSuggestionsInfo info2 =
                new SentenceSuggestionsInfo(
                        new SuggestionsInfo[] {grammarInfo}, new int[] {6}, new int[] {8});

        SpellCheckerSessionBridge bridge = SpellCheckerSessionBridge.create(1234L, true, true);
        bridge.onGetSentenceSuggestions(new SentenceSuggestionsInfo[] {info1, info2});
        watcher.assertExpected();
    }
}
