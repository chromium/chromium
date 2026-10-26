// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.status;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.verify;

import android.app.Activity;
import android.view.View;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.mockito.quality.Strictness;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;

import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.supplier.SettableNonNullObservableSupplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.omnibox.R;
import org.chromium.chrome.browser.omnibox.fusebox.Fusebox.FuseboxState;
import org.chromium.chrome.browser.omnibox.status.StatusMediator.StatusIconType;
import org.chromium.chrome.browser.user_education.IphCommand;
import org.chromium.chrome.browser.user_education.UserEducationHelper;
import org.chromium.components.feature_engagement.FeatureConstants;
import org.chromium.ui.base.TestActivity;

/** Unit tests for {@link FuseboxPlusButtonIphController}. */
@RunWith(BaseRobolectricTestRunner.class)
@NullMarked
public class FuseboxPlusButtonIphControllerUnitTest {
    @Rule
    public final MockitoRule mMockitoRule = MockitoJUnit.rule().strictness(Strictness.STRICT_STUBS);

    @Mock private UserEducationHelper mUserEducationHelper;
    @Captor private ArgumentCaptor<IphCommand> mIphCommandCaptor;

    private final SettableNonNullObservableSupplier<@FuseboxState Integer> mFuseboxStateSupplier =
            ObservableSuppliers.createNonNull(FuseboxState.DISABLED);
    private final SettableNonNullObservableSupplier<@StatusIconType Integer>
            mStatusIconTypeSupplier = ObservableSuppliers.createNonNull(StatusIconType.NONE);

    private View mStatusView;
    private FuseboxPlusButtonIphController mController;

    @Before
    public void setUp() {
        Activity activity = Robolectric.buildActivity(TestActivity.class).setup().get();
        mStatusView = new View(activity);
        activity.setContentView(mStatusView);
        mController =
                new FuseboxPlusButtonIphController(
                        mUserEducationHelper,
                        mStatusView,
                        mFuseboxStateSupplier,
                        mStatusIconTypeSupplier);
    }

    private void setIphShowingState() {
        mFuseboxStateSupplier.set(FuseboxState.COMPACT);
        mStatusIconTypeSupplier.set(StatusIconType.FUSEBOX_PLUS_BUTTON);
    }

    @Test
    public void updateVisibility_compactAndPlusButton_showsIph() {
        setIphShowingState();

        assertTrue(mController.isShowingForTesting());
        verify(mUserEducationHelper).requestShowIph(mIphCommandCaptor.capture());
        IphCommand command = mIphCommandCaptor.getValue();
        command.fetchFromResources();
        assertEquals(FeatureConstants.FUSEBOX_PLUS_BUTTON_HINT_FEATURE, command.featureName);
        assertEquals(mStatusView, command.anchorView);
        assertEquals(R.string.fusebox_attachment_button_iph, command.stringId);
        assertEquals(R.string.fusebox_attachment_button_iph, command.accessibilityStringId);
        assertEquals(
                mStatusView.getContext().getString(R.string.fusebox_attachment_button_iph),
                command.contentString);
    }

    @Test
    public void updateVisibility_unfocusedNtpPlusButton_doesNotShowIph() {
        setIphShowingState();

        mFuseboxStateSupplier.set(FuseboxState.DISABLED);

        assertFalse(mController.isShowingForTesting());
    }

    @Test
    public void updateVisibility_compactWithOtherIcon_doesNotShowIph() {
        setIphShowingState();

        mStatusIconTypeSupplier.set(StatusIconType.PREVIEW_FAVICON);

        assertFalse(mController.isShowingForTesting());
    }

    @Test
    public void updateVisibility_anchorHidden_doesNotShowIph() {
        mStatusView.setVisibility(View.GONE);

        setIphShowingState();

        assertFalse(mController.isShowingForTesting());
    }

    @Test
    @Config(qualifiers = "land")
    public void updateVisibility_landscapeOnPhone_doesNotShowIph() {
        setIphShowingState();

        assertFalse(mController.isShowingForTesting());
    }

    @Test
    @Config(qualifiers = "sw600dp-land")
    public void updateVisibility_landscapeOnTablet_showsIph() {
        setIphShowingState();

        assertTrue(mController.isShowingForTesting());
    }

    @Test
    public void updateVisibility_expandedState_dismissesBubble() {
        setIphShowingState();

        mFuseboxStateSupplier.set(FuseboxState.EXPANDED);
        assertFalse(mController.isShowingForTesting());
    }

    @Test
    public void updateVisibility_onNotShownCallback_resetsShowingState() {
        setIphShowingState();
        verify(mUserEducationHelper).requestShowIph(mIphCommandCaptor.capture());

        mIphCommandCaptor.getValue().onBlockedCallback.run();
        assertFalse(mController.isShowingForTesting());
    }

    @Test
    public void updateVisibility_onDismissCallback_resetsShowingState() {
        setIphShowingState();
        verify(mUserEducationHelper).requestShowIph(mIphCommandCaptor.capture());

        mIphCommandCaptor.getValue().onDismissCallback.run();
        assertFalse(mController.isShowingForTesting());
    }

    @Test
    public void destroy_unregistersObserversAndDismissesBubble() {
        setIphShowingState();

        mController.destroy();
        assertFalse(mController.isShowingForTesting());

        // Subsequent supplier updates after destroy should not trigger IPH.
        mFuseboxStateSupplier.set(FuseboxState.DISABLED);
        setIphShowingState();
        assertFalse(mController.isShowingForTesting());
    }
}
