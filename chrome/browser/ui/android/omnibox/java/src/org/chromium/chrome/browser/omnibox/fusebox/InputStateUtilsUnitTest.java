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
import org.chromium.components.omnibox.IconProto.Icon;
import org.chromium.components.omnibox.IconResourceIdsProto.IconResourceIds;
import org.chromium.components.omnibox.InputSourceProtoIntDef.InputSource;
import org.chromium.components.omnibox.InputTypeConfigProto.InputTypeConfig;
import org.chromium.components.omnibox.ToolConfigProto.ToolConfig;
import org.chromium.components.omnibox.ToolModeProtoIntDef.ToolMode;

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

    private static int resolveIcon(InputState state, @InputSource int source, int defaultIconRes) {
        return InputStateUtils.resolveInputIconRes(
                state, source, defaultIconRes, /* shouldRecordFallback= */ true);
    }

    private static int resolveToolIcon(InputState state, @ToolMode int toolMode) {
        return InputStateUtils.resolveToolIconRes(state, toolMode, R.drawable.image_create_24dp);
    }

    private static InputTypeConfig createLabelConfig(@InputSource int source, String label) {
        return InputTypeConfig.newBuilder().setInputSourceValue(source).setMenuLabel(label).build();
    }

    private static InputTypeConfig createIconConfig(
            @InputSource int source, IconResourceIds iconId) {
        return InputTypeConfig.newBuilder()
                .setInputSourceValue(source)
                .setIcon(Icon.newBuilder().setIconId(iconId))
                .build();
    }

    private static InputState createUnmappedAndEmptyIconState() {
        return new InputStateBuilder()
                .withInputTypeConfigs(
                        createIconConfig(InputSource.INPUT_SOURCE_CAMERA, IconResourceIds.SOCCER),
                        InputTypeConfig.newBuilder()
                                .setInputSourceValue(InputSource.INPUT_SOURCE_FILE_PICKER)
                                .build())
                .build();
    }

    private static ToolConfig createToolIconConfig(@ToolMode int toolMode, IconResourceIds iconId) {
        return ToolConfig.newBuilder()
                .setToolValue(toolMode)
                .setIcon(Icon.newBuilder().setIconId(iconId))
                .build();
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

    @Test
    public void resolveInputIconRes_usesConfiguredIcon() {
        InputState state =
                new InputStateBuilder()
                        .withInputTypeConfigs(
                                createIconConfig(
                                        InputSource.INPUT_SOURCE_GALLERY, IconResourceIds.BOLT))
                        .build();
        HistogramWatcher watcher =
                HistogramWatcher.newBuilder()
                        .expectNoRecords(FuseboxMetrics.ATTACHMENT_ICON_FALLBACK_HISTOGRAM)
                        .build();

        assertEquals(
                R.drawable.bolt_24dp,
                resolveIcon(
                        state,
                        InputSource.INPUT_SOURCE_GALLERY,
                        R.drawable.add_photo_alternate_24dp));
        watcher.assertExpected();
    }

    @Test
    public void resolveInputIconRes_fallsBackWhenNullMissingOrUnmapped() {
        InputState state = createUnmappedAndEmptyIconState();
        HistogramWatcher watcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecords(
                                FuseboxMetrics.ATTACHMENT_ICON_FALLBACK_HISTOGRAM,
                                InputSource.INPUT_SOURCE_DRIVE,
                                InputSource.INPUT_SOURCE_FILE_PICKER,
                                InputSource.INPUT_SOURCE_CAMERA,
                                InputSource.INPUT_SOURCE_GALLERY)
                        .build();

        assertEquals(
                R.drawable.gs_drive_vd_theme_24,
                resolveIcon(null, InputSource.INPUT_SOURCE_DRIVE, R.drawable.gs_drive_vd_theme_24));
        assertEquals(
                R.drawable.ic_attach_file_24dp,
                resolveIcon(
                        state,
                        InputSource.INPUT_SOURCE_FILE_PICKER,
                        R.drawable.ic_attach_file_24dp));
        assertEquals(
                R.drawable.photo_camera_24dp,
                resolveIcon(state, InputSource.INPUT_SOURCE_CAMERA, R.drawable.photo_camera_24dp));
        assertEquals(
                R.drawable.add_photo_alternate_24dp,
                resolveIcon(
                        state,
                        InputSource.INPUT_SOURCE_GALLERY,
                        R.drawable.add_photo_alternate_24dp));
        watcher.assertExpected();
    }

    @Test
    public void resolveInputIconRes_skipsHistogramWhenRecordingDisabled() {
        HistogramWatcher watcher =
                HistogramWatcher.newBuilder()
                        .expectNoRecords(FuseboxMetrics.ATTACHMENT_ICON_FALLBACK_HISTOGRAM)
                        .build();

        assertEquals(
                R.drawable.add_photo_alternate_24dp,
                InputStateUtils.resolveInputIconRes(
                        /* inputState= */ null,
                        InputSource.INPUT_SOURCE_GALLERY,
                        R.drawable.add_photo_alternate_24dp,
                        /* shouldRecordFallback= */ false));
        watcher.assertExpected();
    }

    @Test
    public void resolveToolIconRes_usesConfiguredIcon() {
        InputState state =
                new InputStateBuilder()
                        .withToolConfigs(
                                createToolIconConfig(
                                        ToolMode.TOOL_MODE_DEEP_SEARCH,
                                        IconResourceIds.TRAVEL_EXPLORE),
                                createToolIconConfig(
                                        ToolMode.TOOL_MODE_IMAGE_GEN, IconResourceIds.BANANA))
                        .build();
        HistogramWatcher watcher =
                HistogramWatcher.newBuilder()
                        .expectNoRecords(FuseboxMetrics.ATTACHMENT_ICON_FALLBACK_HISTOGRAM)
                        .build();

        assertEquals(
                R.drawable.create_image_24dp, resolveToolIcon(state, ToolMode.TOOL_MODE_IMAGE_GEN));
        watcher.assertExpected();
    }

    @Test
    public void resolveToolIconRes_fallsBackWhenNullMissingOrUnmapped() {
        int defaultIcon = R.drawable.image_create_24dp;
        InputState state =
                new InputStateBuilder()
                        .withToolConfigs(
                                createToolIconConfig(
                                        ToolMode.TOOL_MODE_DEEP_SEARCH, IconResourceIds.SOCCER),
                                ToolConfig.newBuilder()
                                        .setToolValue(ToolMode.TOOL_MODE_IMAGE_GEN)
                                        .build())
                        .build();
        HistogramWatcher watcher =
                HistogramWatcher.newBuilder()
                        .expectNoRecords(FuseboxMetrics.ATTACHMENT_ICON_FALLBACK_HISTOGRAM)
                        .build();

        assertEquals(defaultIcon, resolveToolIcon(null, ToolMode.TOOL_MODE_IMAGE_GEN));
        assertEquals(defaultIcon, resolveToolIcon(state, ToolMode.TOOL_MODE_CANVAS));
        assertEquals(defaultIcon, resolveToolIcon(state, ToolMode.TOOL_MODE_IMAGE_GEN));
        assertEquals(defaultIcon, resolveToolIcon(state, ToolMode.TOOL_MODE_DEEP_SEARCH));
        watcher.assertExpected();
    }
}
