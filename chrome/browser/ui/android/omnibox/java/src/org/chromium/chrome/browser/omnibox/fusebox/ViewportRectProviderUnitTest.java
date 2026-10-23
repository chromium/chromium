// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.fusebox;

import static org.junit.Assert.assertEquals;
import static org.mockito.Mockito.clearInvocations;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.content.res.Configuration;
import android.graphics.Rect;
import android.os.Build;
import android.view.View;

import androidx.core.graphics.Insets;
import androidx.core.view.ViewCompat;
import androidx.core.view.WindowInsetsCompat;
import androidx.core.view.WindowInsetsCompat.Type;
import androidx.window.layout.WindowMetrics;
import androidx.window.layout.WindowMetricsCalculator;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.mockito.quality.Strictness;
import org.robolectric.Robolectric;
import org.robolectric.android.controller.ActivityController;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.ResettersForTesting;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.ui.base.ImmutableWeakReference;
import org.chromium.ui.insets.InsetObserver;
import org.chromium.ui.widget.RectProvider;

/** Unit tests for {@link ViewportRectProvider}. */
@RunWith(BaseRobolectricTestRunner.class)
public class ViewportRectProviderUnitTest {
    private static final int INITIAL_WIDTH = 400;
    private static final int INITIAL_HEIGHT = 800;
    private static final int UPDATED_WIDTH = 600;
    private static final int UPDATED_HEIGHT = 900;
    private static final int WINDOW_OFFSET = 150;
    private static final int STATUS_BAR_TOP = 48;
    private static final int IGNORED_INSET = 100;
    private static final float DENSITY = 1.0f;

    @Rule
    public final MockitoRule mMockitoRule = MockitoJUnit.rule().strictness(Strictness.STRICT_STUBS);

    @Mock private RectProvider.Observer mObserver;
    @Mock private WindowMetricsCalculator mWindowMetricsCalculator;

    private ActivityController<Activity> mActivityController;
    private Activity mActivity;
    private View mView;
    private InsetObserver mInsetObserver;
    private ViewportRectProvider mProvider;

    private final Rect mWindowBounds = new Rect();

    @Before
    public void setUp() {
        WindowMetricsCalculator.overrideDecorator(_ -> mWindowMetricsCalculator);
        ResettersForTesting.register(WindowMetricsCalculator::reset);

        mActivityController = Robolectric.buildActivity(Activity.class).create();
        mActivity = mActivityController.get();
        mView = new View(mActivity);
        mInsetObserver =
                new InsetObserver(
                        new ImmutableWeakReference<>(mView),
                        new ImmutableWeakReference<>(mActivity),
                        /* enableKeyboardOverlayMode= */ false,
                        /* enableExtraEdgeToEdgeLogging= */ false);

        mWindowBounds.set(0, 0, INITIAL_WIDTH, INITIAL_HEIGHT);
        when(mWindowMetricsCalculator.computeCurrentWindowMetrics(mActivity))
                .thenAnswer(_ -> new WindowMetrics(new Rect(mWindowBounds), DENSITY));

        mProvider = new ViewportRectProvider(mActivity, mInsetObserver, mView);
        mProvider.startObserving(mObserver);
    }

    @Test
    public void testConstructor_initializesRectAndHandlesNullInsetObserver() {
        dispatchStatusBarInset(STATUS_BAR_TOP);
        ViewportRectProvider providerWithInsets =
                new ViewportRectProvider(mActivity, mInsetObserver, mView);
        assertEquals(
                new Rect(0, STATUS_BAR_TOP, INITIAL_WIDTH, INITIAL_HEIGHT),
                providerWithInsets.getRect());
        providerWithInsets.destroy();

        ViewportRectProvider providerWithoutObserver =
                new ViewportRectProvider(mActivity, /* insetObserver= */ null, mView);
        assertEquals(
                new Rect(0, 0, INITIAL_WIDTH, INITIAL_HEIGHT), providerWithoutObserver.getRect());
        providerWithoutObserver.destroy();
    }

    @Test
    public void testOnInsetChanged_usesOnlyTopSystemBarAndDeduplicatesOtherInsets() {
        assertEquals(new Rect(0, 0, INITIAL_WIDTH, INITIAL_HEIGHT), mProvider.getRect());

        WindowInsetsCompat nonImeInsets =
                new WindowInsetsCompat.Builder()
                        .setInsets(Type.statusBars(), Insets.of(0, STATUS_BAR_TOP, 0, 0))
                        .setInsets(
                                Type.navigationBars(),
                                Insets.of(IGNORED_INSET, 0, IGNORED_INSET, IGNORED_INSET))
                        .setInsets(Type.displayCutout(), Insets.of(0, IGNORED_INSET, 0, 0))
                        .build();
        ViewCompat.dispatchApplyWindowInsets(mView, nonImeInsets);
        assertEquals(
                new Rect(0, STATUS_BAR_TOP, INITIAL_WIDTH, INITIAL_HEIGHT), mProvider.getRect());
        verify(mObserver).onRectChanged();

        clearInvocations(mObserver);
        WindowInsetsCompat imeInsets =
                new WindowInsetsCompat.Builder(nonImeInsets)
                        .setInsets(Type.ime(), Insets.of(0, 0, 0, IGNORED_INSET))
                        .build();
        ViewCompat.dispatchApplyWindowInsets(mView, imeInsets);
        assertEquals(
                new Rect(0, STATUS_BAR_TOP, INITIAL_WIDTH, INITIAL_HEIGHT), mProvider.getRect());
        verify(mObserver, never()).onRectChanged();
    }

    @Test
    public void testOnConfigurationChanged_updatesNormalizedWindowBounds() {
        mWindowBounds.set(
                WINDOW_OFFSET,
                WINDOW_OFFSET,
                WINDOW_OFFSET + UPDATED_WIDTH,
                WINDOW_OFFSET + UPDATED_HEIGHT);
        dispatchConfigurationChange();

        assertEquals(new Rect(0, 0, UPDATED_WIDTH, UPDATED_HEIGHT), mProvider.getRect());
        verify(mObserver).onRectChanged();
    }

    @Test
    public void testOnLayoutChange_defersUpdateUntilUiTaskRuns() {
        mWindowBounds.set(0, 0, UPDATED_WIDTH, UPDATED_HEIGHT);
        mView.layout(0, 0, INITIAL_WIDTH, INITIAL_HEIGHT);
        mView.layout(0, 0, UPDATED_WIDTH, UPDATED_HEIGHT);

        assertEquals(new Rect(0, 0, INITIAL_WIDTH, INITIAL_HEIGHT), mProvider.getRect());
        verify(mObserver, never()).onRectChanged();

        ShadowLooper.idleMainLooper();
        assertEquals(new Rect(0, 0, UPDATED_WIDTH, UPDATED_HEIGHT), mProvider.getRect());
        verify(mObserver).onRectChanged();
    }

    @Test
    public void testDestroy_unregistersAllCallbacks() {
        mProvider.destroy();

        mWindowBounds.set(0, 0, UPDATED_WIDTH, UPDATED_HEIGHT);
        dispatchStatusBarInset(STATUS_BAR_TOP);
        dispatchConfigurationChange();
        mView.layout(0, 0, UPDATED_WIDTH, UPDATED_HEIGHT);
        ShadowLooper.idleMainLooper();

        assertEquals(new Rect(0, 0, INITIAL_WIDTH, INITIAL_HEIGHT), mProvider.getRect());
        verify(mObserver, never()).onRectChanged();
    }

    private void dispatchConfigurationChange() {
        Configuration config = new Configuration(mActivity.getResources().getConfiguration());
        // Activity gained per-instance ComponentCallbacks in API 31. Earlier SDKs delegate
        // Activity#registerComponentCallbacks to the base Application context.
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
            mActivityController.configurationChange(config);
        } else {
            mActivity.getApplication().onConfigurationChanged(config);
        }
    }

    private void dispatchStatusBarInset(int statusBarTop) {
        WindowInsetsCompat insets =
                new WindowInsetsCompat.Builder()
                        .setInsets(Type.statusBars(), Insets.of(0, statusBarTop, 0, 0))
                        .build();
        ViewCompat.dispatchApplyWindowInsets(mView, insets);
    }
}
