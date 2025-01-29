// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.android.whats_new;

import static org.junit.Assert.assertEquals;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.verify;

import android.content.Context;
import android.widget.ViewFlipper;

import androidx.annotation.DrawableRes;

import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.ContextUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.ui.android.whats_new.WhatsNewProperties.ViewState;
import org.chromium.chrome.browser.ui.android.whats_new.features.WhatsNewFeature;
import org.chromium.chrome.browser.ui.android.whats_new.features.WhatsNewFeatureProvider;
import org.chromium.chrome.browser.ui.android.whats_new.features.WhatsNewFeatureUtils.WhatsNewType;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.List;

/** Tests {@link WhatsNewCoordinator}. */
@RunWith(BaseRobolectricTestRunner.class)
@EnableFeatures({ChromeFeatureList.CLANK_WHATS_NEW})
public class WhatsNewCoordinatorTest {
    public @Rule MockitoRule mockitoRule = MockitoJUnit.rule();

    @Mock private BottomSheetController mBottomSheetControllerMock;

    public static class TestFeature implements WhatsNewFeature {
        @Override
        public @WhatsNewType int getType() {
            return WhatsNewType.EXAMPLE_FEATURE;
        }

        @Override
        public String getName() {
            return "TestFeature";
        }

        @Override
        public String getTitle(Context context) {
            return "TestTitle";
        }

        @Override
        public String getDescription(Context context) {
            return "TestDescription";
        }

        @Override
        public @DrawableRes int getIconResId() {
            return 0;
        }
    }

    @Test
    public void testShowBottomSheet() {
        WhatsNewCoordinator coordinator =
                new WhatsNewCoordinator(
                        ContextUtils.getApplicationContext(), mBottomSheetControllerMock);

        coordinator.showBottomSheet();

        assertEquals(
                ViewState.OVERVIEW,
                coordinator.getModelForTesting().get(WhatsNewProperties.VIEW_STATE));
        verify(mBottomSheetControllerMock).requestShowContent(any(), eq(true));
    }

    @Test
    public void testSetViewState() {
        WhatsNewCoordinator coordinator =
                new WhatsNewCoordinator(
                        ContextUtils.getApplicationContext(), mBottomSheetControllerMock);
        ViewFlipper viewFlipperView =
                (ViewFlipper) coordinator.getView().findViewById(R.id.whats_new_page_view_flipper);

        coordinator.getModelForTesting().set(WhatsNewProperties.VIEW_STATE, ViewState.OVERVIEW);
        verify(mBottomSheetControllerMock).requestShowContent(any(), eq(true));
        assertEquals(0, viewFlipperView.getDisplayedChild());

        coordinator.getModelForTesting().set(WhatsNewProperties.VIEW_STATE, ViewState.DETAIL);
        assertEquals(1, viewFlipperView.getDisplayedChild());

        coordinator.getModelForTesting().set(WhatsNewProperties.VIEW_STATE, ViewState.HIDDEN);
        verify(mBottomSheetControllerMock).hideContent(any(), eq(true));
    }

    @Test
    public void testFeatureItemList() {
        WhatsNewFeatureProvider.setFeatureListForTests(List.of(new TestFeature()));
        WhatsNewCoordinator coordinator =
                new WhatsNewCoordinator(
                        ContextUtils.getApplicationContext(), mBottomSheetControllerMock);

        ModelList modelList = coordinator.getModelListForTesting();
        assertEquals(1, modelList.size());
        PropertyModel model = modelList.get(0).model;
        assertEquals("TestTitle", model.get(WhatsNewListItemProperties.TITLE_ID));
        assertEquals("TestDescription", model.get(WhatsNewListItemProperties.DESCRIPTION_ID));
    }
}
