// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.webapps.pwa_restore_ui;

import android.app.Activity;
import android.graphics.Bitmap;
import android.graphics.Color;
import android.view.View;
import android.view.ViewGroup;
import android.widget.LinearLayout;
import android.widget.LinearLayout.LayoutParams;

import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.BeforeClass;
import org.junit.ClassRule;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.MockitoAnnotations;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.BaseActivityTestRule;
import org.chromium.base.test.params.BaseJUnit4RunnerDelegate;
import org.chromium.base.test.params.ParameterAnnotations.ClassParameter;
import org.chromium.base.test.params.ParameterAnnotations.UseRunnerDelegate;
import org.chromium.base.test.params.ParameterSet;
import org.chromium.base.test.params.ParameterizedRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.JniMocker;
import org.chromium.components.browser_ui.widget.test.R;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.test.util.BlankUiTestActivity;
import org.chromium.ui.test.util.NightModeTestUtils;
import org.chromium.ui.test.util.RenderTestRule;

import java.util.ArrayList;
import java.util.List;

/** Render test for {@link PwaRestoreBottomSheetView}. */
@RunWith(ParameterizedRunner.class)
@UseRunnerDelegate(BaseJUnit4RunnerDelegate.class)
@Batch(Batch.UNIT_TESTS)
public class PwaRestoreBottomSheetViewRenderTest {
    private static Activity sActivity;

    @ClassParameter
    private static List<ParameterSet> sClassParams =
            new NightModeTestUtils.NightModeParams().getParameters();

    @ClassRule
    public static BaseActivityTestRule<BlankUiTestActivity> sActivityTestRule =
            new BaseActivityTestRule<>(BlankUiTestActivity.class);

    @Rule
    public RenderTestRule mRenderTestRule =
            RenderTestRule.Builder.withPublicCorpus()
                    .setCorpus(RenderTestRule.Corpus.ANDROID_RENDER_TESTS_PUBLIC)
                    .setBugComponent(RenderTestRule.Component.UI_BROWSER_WEB_APP_INSTALLS)
                    .build();

    @Rule public JniMocker mocker = new JniMocker();

    @BeforeClass
    public static void setupSuite() {
        sActivityTestRule.launchActivity(null);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    sActivity = sActivityTestRule.getActivity();
                });
    }

    @Mock private PwaRestoreBottomSheetMediator.Natives mNativeMock;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mocker.mock(PwaRestoreBottomSheetMediatorJni.TEST_HOOKS, mNativeMock);
        Mockito.when(mNativeMock.initialize(Mockito.any())).thenReturn(0L);
    }

    public PwaRestoreBottomSheetViewRenderTest(boolean nightModeEnabled) {
        NightModeTestUtils.setUpNightModeForBlankUiTestActivity(nightModeEnabled);
        mRenderTestRule.setNightModeEnabled(nightModeEnabled);
        mNightModeEnabled = nightModeEnabled;
    }

    private PwaRestoreBottomSheetCoordinator mCoordinator;
    private PropertyModel mModel;

    private final boolean mNightModeEnabled;

    private static Bitmap createBitmap(int color) {
        int[] colors = {color};
        return Bitmap.createBitmap(colors, 1, 1, Bitmap.Config.ALPHA_8);
    }

    private void initializeBottomSheet() {
        String[] appIds = new String[] {"foo", "bar", "foobar"};
        String[] appNames = new String[] {"Foo", "Bar", "Barfoo"};
        List<Bitmap> appIcons = new ArrayList<Bitmap>();
        appIcons.add(createBitmap(Color.RED));
        appIcons.add(createBitmap(Color.GREEN));
        appIcons.add(createBitmap(Color.BLUE));
        int[] lastUsedList = new int[] {1, 2, 3};

        mCoordinator =
                new PwaRestoreBottomSheetCoordinator(
                        appIds,
                        appNames,
                        appIcons,
                        lastUsedList,
                        sActivity,
                        null,
                        R.drawable.ic_arrow_back_24dp);
        PropertyModel model = mCoordinator.getModelForTesting();
        model.set(PwaRestoreProperties.VIEW_STATE, PwaRestoreProperties.ViewState.PREVIEW);

        LinearLayout content = new LinearLayout(sActivity);
        sActivity.setContentView(content);
                    View view = mCoordinator.getBottomSheetViewForTesting();
                    View root = view.getRootView();
                    root.setBackgroundColor(mNightModeEnabled ? Color.BLACK : Color.WHITE);

        content.addView(
                view,
                new LayoutParams(
                        ViewGroup.LayoutParams.WRAP_CONTENT, ViewGroup.LayoutParams.WRAP_CONTENT));
    }

    @Test
    @SmallTest
    @Feature({"RenderTest"})
    public void testPeeking() throws Exception {
        ThreadUtils.runOnUiThreadBlocking(this::initializeBottomSheet);
        mRenderTestRule.render(mCoordinator.getBottomSheetViewForTesting(), "pwa_restore_peeking");
    }
}
