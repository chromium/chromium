// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.fusebox;

import static org.junit.Assert.assertEquals;

import android.content.Context;

import androidx.test.core.app.ApplicationProvider;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.chrome.browser.omnibox.R;
import org.chromium.components.contextual_search.InputState;
import org.chromium.components.contextual_search.InputStateBuilder;
import org.chromium.components.omnibox.InputSourceProtoIntDef.InputSource;
import org.chromium.components.omnibox.InputTypeConfigProto.InputTypeConfig;

/** Unit tests for {@link InputStateUtils}. */
@RunWith(BaseRobolectricTestRunner.class)
public class InputStateUtilsUnitTest {
    private Context mContext;

    @Before
    public void setUp() {
        mContext = ApplicationProvider.getApplicationContext();
    }

    private String resolveLabel(InputState state, @InputSource int source, int defaultTextRes) {
        return InputStateUtils.resolveInputMenuLabel(
                mContext, state, source, defaultTextRes, /* shouldRecordFallback= */ true);
    }

    private static InputTypeConfig createLabelConfig(@InputSource int source, String label) {
        return InputTypeConfig.newBuilder().setInputSourceValue(source).setMenuLabel(label).build();
    }

    @Test
    public void resolveInputMenuLabel_usesConfiguredMenuLabel() {
        InputState state =
                new InputStateBuilder()
                        .withInputTypeConfigs(
                                createLabelConfig(InputSource.INPUT_SOURCE_CAMERA, "Snap photo"),
                                createLabelConfig(InputSource.INPUT_SOURCE_GALLERY, "Photos"))
                        .build();
        HistogramWatcher watcher =
                HistogramWatcher.newBuilder()
                        .expectNoRecords(FuseboxMetrics.ATTACHMENT_LABEL_FALLBACK_HISTOGRAM)
                        .build();

        assertEquals(
                "Photos",
                resolveLabel(
                        state,
                        InputSource.INPUT_SOURCE_GALLERY,
                        R.string.omnibox_navattach_gallery));
        assertEquals(
                "Snap photo",
                resolveLabel(state, InputSource.INPUT_SOURCE_CAMERA, R.string.photo_picker_camera));
        watcher.assertExpected();
    }

    @Test
    public void resolveInputMenuLabel_fallsBackWhenNullMissingOrEmpty() {
        int galleryRes = R.string.omnibox_navattach_gallery;
        String defaultGallery = mContext.getString(galleryRes);
        InputState emptyLabelState =
                new InputStateBuilder()
                        .withInputTypeConfigs(
                                createLabelConfig(InputSource.INPUT_SOURCE_GALLERY, ""))
                        .build();
        HistogramWatcher watcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecords(
                                FuseboxMetrics.ATTACHMENT_LABEL_FALLBACK_HISTOGRAM,
                                InputSource.INPUT_SOURCE_GALLERY,
                                InputSource.INPUT_SOURCE_GALLERY,
                                InputSource.INPUT_SOURCE_DRIVE)
                        .build();

        assertEquals(
                defaultGallery, resolveLabel(null, InputSource.INPUT_SOURCE_GALLERY, galleryRes));
        assertEquals(
                defaultGallery,
                resolveLabel(emptyLabelState, InputSource.INPUT_SOURCE_GALLERY, galleryRes));
        assertEquals(
                mContext.getString(R.string.omnibox_navattach_drive),
                resolveLabel(
                        emptyLabelState,
                        InputSource.INPUT_SOURCE_DRIVE,
                        R.string.omnibox_navattach_drive));
        watcher.assertExpected();
    }

    @Test
    public void resolveInputMenuLabel_skipsHistogramWhenRecordingDisabled() {
        HistogramWatcher watcher =
                HistogramWatcher.newBuilder()
                        .expectNoRecords(FuseboxMetrics.ATTACHMENT_LABEL_FALLBACK_HISTOGRAM)
                        .build();

        assertEquals(
                mContext.getString(R.string.omnibox_navattach_gallery),
                InputStateUtils.resolveInputMenuLabel(
                        mContext,
                        /* inputState= */ null,
                        InputSource.INPUT_SOURCE_GALLERY,
                        R.string.omnibox_navattach_gallery,
                        /* shouldRecordFallback= */ false));
        watcher.assertExpected();
    }
}
