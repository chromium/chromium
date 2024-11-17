// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.status;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.assertion.ViewAssertions.doesNotExist;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.ViewMatchers.isCompletelyDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withId;

import static org.hamcrest.Matchers.not;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;

import static org.chromium.base.ThreadUtils.runOnUiThreadBlocking;

import android.animation.Animator;
import android.graphics.drawable.Drawable;
import android.view.View;
import android.view.ViewGroup;
import android.widget.FrameLayout;
import android.widget.ImageView;
import android.widget.LinearLayout;

import androidx.test.filters.MediumTest;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.MockitoAnnotations;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Restriction;
import org.chromium.base.test.util.TestAnimations.EnableAnimations;
import org.chromium.chrome.browser.omnibox.R;
import org.chromium.chrome.browser.omnibox.status.StatusProperties.StatusIconResource;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.components.browser_ui.widget.ChromeTransitionDrawable;
import org.chromium.components.browser_ui.widget.CompositeTouchDelegate;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;
import org.chromium.ui.test.util.BlankUiTestActivityTestCase;

import java.lang.reflect.InvocationTargetException;
import java.util.concurrent.ExecutionException;

/** Tests for {@link StatusView} and {@link StatusViewBinder}. */
@RunWith(ChromeJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
public class StatusViewTest extends BlankUiTestActivityTestCase {
    private StatusView mStatusView;
    private PropertyModel mStatusModel;
    private PropertyModelChangeProcessor mStatusMCP;

    @Override
    public void setUpTest() throws Exception {
        super.setUpTest();
        MockitoAnnotations.initMocks(this);

        runOnUiThreadBlocking(
                () -> {
                    ViewGroup view = new LinearLayout(getActivity());

                    FrameLayout.LayoutParams params =
                            new FrameLayout.LayoutParams(
                                    ViewGroup.LayoutParams.MATCH_PARENT,
                                    ViewGroup.LayoutParams.MATCH_PARENT);

                    getActivity().setContentView(view, params);

                    mStatusView =
                            getActivity()
                                    .getLayoutInflater()
                                    .inflate(R.layout.location_status, view, true)
                                    .findViewById(R.id.location_bar_status);
                    mStatusView.setCompositeTouchDelegate(new CompositeTouchDelegate(view));
                    mStatusModel = new PropertyModel.Builder(StatusProperties.ALL_KEYS).build();
                    mStatusMCP =
                            PropertyModelChangeProcessor.create(
                                    mStatusModel, mStatusView, new StatusViewBinder());
                });
    }

    @Override
    public void tearDownTest() throws Exception {
        ThreadUtils.runOnUiThreadBlocking(mStatusMCP::destroy);
        super.tearDownTest();
    }

    @Test
    @MediumTest
    @Feature({"Omnibox"})
    public void testIncognitoBadgeVisibility() {
        // Verify that the incognito badge is not inflated by default.
        assertFalse(mStatusModel.get(StatusProperties.INCOGNITO_BADGE_VISIBLE));
        onView(withId(R.id.location_bar_incognito_badge)).check(doesNotExist());

        // Set incognito badge visible.
        runOnUiThreadBlocking(
                () -> {
                    mStatusModel.set(StatusProperties.INCOGNITO_BADGE_VISIBLE, true);
                });
        onView(withId(R.id.location_bar_incognito_badge)).check(matches(isCompletelyDisplayed()));

        // Set incognito badge gone.
        runOnUiThreadBlocking(
                () -> {
                    mStatusModel.set(StatusProperties.INCOGNITO_BADGE_VISIBLE, false);
                });
        onView(withId(R.id.location_bar_incognito_badge)).check(matches(not(isDisplayed())));
    }

    @Test
    @MediumTest
    @Feature({"Omnibox"})
    public void testTouchDelegate_nullWhenIncognitoStatusIconInvisible() {
        // Verify that the incognito badge is not inflated by default.
        assertFalse(mStatusModel.get(StatusProperties.INCOGNITO_BADGE_VISIBLE));
        onView(withId(R.id.location_bar_incognito_badge)).check(doesNotExist());

        // Set incognito badge visible.
        runOnUiThreadBlocking(
                () -> {
                    mStatusModel.set(StatusProperties.INCOGNITO_BADGE_VISIBLE, true);
                });
        onView(withId(R.id.location_bar_incognito_badge)).check(matches(isCompletelyDisplayed()));

        runOnUiThreadBlocking(
                () -> {
                    mStatusModel.set(StatusProperties.STATUS_ICON_RESOURCE, null);
                });
        onView(withId(R.id.location_bar_status_icon))
                .check((view, e) -> assertNull(mStatusView.getTouchDelegateForTesting()));
    }

    @Test
    @MediumTest
    @Feature({"Omnibox"})
    public void testTouchDelegate_notNullWhenIncognitoStatusIconVisible() {
        // Verify that the incognito badge is not inflated by default.
        assertFalse(mStatusModel.get(StatusProperties.INCOGNITO_BADGE_VISIBLE));
        onView(withId(R.id.location_bar_incognito_badge)).check(doesNotExist());

        // Set incognito badge visible.
        runOnUiThreadBlocking(
                () -> {
                    mStatusModel.set(StatusProperties.INCOGNITO_BADGE_VISIBLE, true);
                });
        onView(withId(R.id.location_bar_incognito_badge)).check(matches(isCompletelyDisplayed()));

        runOnUiThreadBlocking(
                () -> {
                    mStatusModel.set(
                            StatusProperties.STATUS_ICON_RESOURCE,
                            new StatusIconResource(R.drawable.ic_search, 0));
                });
        onView(withId(R.id.location_bar_status_icon))
                .check((view, e) -> assertNotNull(mStatusView.getTouchDelegateForTesting()));
    }

    @Test
    @MediumTest
    @Restriction(DeviceFormFactor.PHONE)
    @Feature({"Omnibox"})
    public void statusView_goneWhenIncognitoBadgeVisible() {
        // Set location_bar_status_icon is VISIBLE in the beginning.
        runOnUiThreadBlocking(
                () -> {
                    mStatusModel.set(
                            StatusProperties.STATUS_ICON_RESOURCE,
                            new StatusIconResource(R.drawable.ic_search, 0));
                });
        onView(withId(R.id.location_bar_status_icon_frame))
                .check(
                        (view, e) -> {
                            assertEquals(View.VISIBLE, view.getVisibility());
                        });

        // Verify that the incognito badge is not inflated by default.
        assertFalse(mStatusModel.get(StatusProperties.INCOGNITO_BADGE_VISIBLE));
        onView(withId(R.id.location_bar_incognito_badge)).check(doesNotExist());

        // Set incognito badge visible.
        runOnUiThreadBlocking(
                () -> {
                    mStatusModel.set(StatusProperties.INCOGNITO_BADGE_VISIBLE, true);
                });
        onView(withId(R.id.location_bar_incognito_badge)).check(matches(isCompletelyDisplayed()));

        runOnUiThreadBlocking(
                () -> {
                    mStatusModel.set(StatusProperties.STATUS_ICON_RESOURCE, null);
                });
        onView(withId(R.id.location_bar_status_icon_frame))
                .check(
                        (view, e) -> {
                            assertEquals(View.GONE, view.getVisibility());
                        });
    }

    @Test
    @MediumTest
    @Restriction(DeviceFormFactor.PHONE)
    @Feature({"Omnibox"})
    public void testSearchEngineLogo_incognito_noMarginEnd() {
        // Set incognito badge visible.
        runOnUiThreadBlocking(
                () -> {
                    mStatusModel.set(StatusProperties.INCOGNITO_BADGE_VISIBLE, true);
                });
        onView(withId(R.id.location_bar_incognito_badge)).check(matches(isCompletelyDisplayed()));

        runOnUiThreadBlocking(
                () -> {
                    mStatusModel.set(
                            StatusProperties.STATUS_ICON_RESOURCE,
                            new StatusIconResource(R.drawable.ic_logo_googleg_24dp, 0));
                });
        onView(withId(R.id.location_bar_incognito_badge))
                .check(
                        (view, e) -> {
                            ViewGroup.MarginLayoutParams params =
                                    (ViewGroup.MarginLayoutParams) view.getLayoutParams();
                            assertEquals(0, params.getMarginEnd());
                        });
    }

    @Test
    @MediumTest
    @Feature({"Omnibox"})
    public void testStatusViewAnimationStatusResetOnHide() {
        runOnUiThreadBlocking(
                () -> {
                    mStatusModel.set(StatusProperties.SHOW_STATUS_ICON, true);
                    mStatusModel.set(
                            StatusProperties.STATUS_ICON_RESOURCE,
                            new StatusIconResource(R.drawable.ic_logo_googleg_24dp, 0));
                    assertTrue(mStatusView.isStatusIconAnimating());
                    mStatusModel.set(StatusProperties.SHOW_STATUS_ICON, false);
                    assertFalse(mStatusView.isStatusIconAnimating());
                });
    }

    @Test
    @MediumTest
    @Feature({"Omnibox"})
    public void testStatusView_iconTransparencyShouldBeReset() {
        StatusIconResource statusIconResource =
                new StatusIconResource(R.drawable.ic_logo_googleg_24dp, 0);
        runOnUiThreadBlocking(
                () -> {
                    mStatusModel.set(StatusProperties.SHOW_STATUS_ICON, true);
                    mStatusModel.set(StatusProperties.STATUS_ICON_RESOURCE, statusIconResource);
                });

        // Hide the icon, this starts an animation to set alpha to 0.0.
        runOnUiThreadBlocking(() -> mStatusModel.set(StatusProperties.STATUS_ICON_RESOURCE, null));

        // Show the icon again, the alpha property should be reset to 1.0.
        runOnUiThreadBlocking(
                () -> mStatusModel.set(StatusProperties.STATUS_ICON_RESOURCE, statusIconResource));

        onView(withId(R.id.location_bar_status_icon_frame))
                .check(
                        (view, e) -> {
                            assertEquals(View.VISIBLE, view.getVisibility());
                            assertEquals(1.0, view.getAlpha(), 0.0);
                        });
    }

    @Test
    @MediumTest
    @Feature({"Omnibox"})
    public void testStatusViewAnimationStatusResetAfterDuration()
            throws ExecutionException, InterruptedException {
        runOnUiThreadBlocking(
                () -> {
                    mStatusView.setIconAnimationDurationForTesting(50);
                    mStatusModel.set(StatusProperties.SHOW_STATUS_ICON, true);
                    mStatusModel.set(StatusProperties.ANIMATIONS_ENABLED, true);
                    mStatusModel.set(
                            StatusProperties.STATUS_ICON_RESOURCE,
                            new StatusIconResource(R.drawable.ic_logo_googleg_24dp, 0));
                    assertTrue(mStatusView.isStatusIconAnimating());
                });

        CriteriaHelper.pollUiThread(() -> !mStatusView.isStatusIconAnimating(), 300, 20);
    }

    @Test
    @MediumTest
    @Feature({"Omnibox"})
    @EnableAnimations
    public void testStatusViewAnimation_noConcurrentAnimation()
            throws ExecutionException,
                    InterruptedException,
                    InvocationTargetException,
                    IllegalAccessException {
        runOnUiThreadBlocking(
                () -> {
                    mStatusView.setIconAnimationDurationForTesting(100);
                    mStatusModel.set(StatusProperties.SHOW_STATUS_ICON, true);
                    mStatusModel.set(StatusProperties.ANIMATIONS_ENABLED, true);
                    mStatusModel.set(
                            StatusProperties.STATUS_ICON_RESOURCE,
                            new StatusIconResource(R.drawable.ic_logo_googleg_24dp, 0));
                    assertTrue(mStatusView.isStatusIconAnimating());
                    ChromeTransitionDrawable initialTransitionDrawable =
                            (ChromeTransitionDrawable)
                                    ((ImageView) mStatusView.getSecurityView()).getDrawable();
                    Animator initialAnimator = initialTransitionDrawable.getAnimatorForTesting();
                    assertTrue(
                            "Initial transition drawable should be animating",
                            initialAnimator.isStarted());
                    assertTrue(
                            "Initial transition drawable should be animating",
                            initialAnimator.isRunning());
                    Drawable finalDrawable = initialTransitionDrawable.getFinalDrawable();

                    mStatusView.setIconAnimationDurationForTesting(0);
                    mStatusModel.set(
                            StatusProperties.STATUS_ICON_RESOURCE,
                            new StatusIconResource(R.drawable.ic_search, 0));

                    assertFalse(
                            "Initial transition drawable should have stopped animating",
                            initialAnimator.isStarted());
                    assertFalse(
                            "Initial transition drawable should have stopped animating",
                            initialAnimator.isRunning());
                    assertEquals(255, finalDrawable.getAlpha());
                    assertTrue(mStatusView.isStatusIconAnimating());
                    ChromeTransitionDrawable nextTransitionDrawable =
                            (ChromeTransitionDrawable)
                                    ((ImageView) mStatusView.getSecurityView()).getDrawable();
                    assertTrue(nextTransitionDrawable.getAnimatorForTesting().isStarted());
                });
    }

    @Test
    @MediumTest
    @Feature({"Omnibox"})
    public void testShowStatusViewToggleVisibility()
            throws ExecutionException, InterruptedException {
        runOnUiThreadBlocking(
                () -> {
                    mStatusModel.set(StatusProperties.SHOW_STATUS_VIEW, true);
                    assertEquals(View.VISIBLE, mStatusView.getVisibility());
                    mStatusModel.set(StatusProperties.SHOW_STATUS_VIEW, false);
                    assertEquals(View.GONE, mStatusView.getVisibility());
                });
    }

    @Test
    @MediumTest
    @Feature({"Omnibox"})
    public void testShowStatusViewNotAffectedByShowIconView()
            throws ExecutionException, InterruptedException {
        runOnUiThreadBlocking(
                () -> {
                    mStatusModel.set(StatusProperties.SHOW_STATUS_VIEW, false);
                    assertEquals(View.GONE, mStatusView.getVisibility());
                    mStatusModel.set(StatusProperties.SHOW_STATUS_ICON, true);
                    assertEquals(View.GONE, mStatusView.getVisibility());
                });
    }
}
