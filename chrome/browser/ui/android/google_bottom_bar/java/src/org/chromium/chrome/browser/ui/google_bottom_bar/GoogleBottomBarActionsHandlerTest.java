// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.google_bottom_bar;

import static org.junit.Assert.assertEquals;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.app.PendingIntent;
import android.content.Context;
import android.content.Intent;
import android.net.Uri;
import android.view.View;

import androidx.test.ext.junit.rules.ActivityScenarioRule;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowLog;

import org.chromium.base.supplier.Supplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.chrome.browser.page_insights.PageInsightsCoordinator;
import org.chromium.chrome.browser.share.ShareDelegate;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.ui.google_bottom_bar.GoogleBottomBarLogger.GoogleBottomBarButtonEvent;
import org.chromium.components.browser_ui.widget.textbubble.TextBubble;
import org.chromium.ui.base.TestActivity;
import org.chromium.url.GURL;

import java.util.Set;

/** Unit tests for {@link BottomBarConfig}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(
        manifest = Config.NONE,
        shadows = {ShadowLog.class})
public class GoogleBottomBarActionsHandlerTest {
    private static final String TEST_URI = "https://www.test.com/";
    private final GURL mGURL = new GURL(TEST_URI);

    @Rule
    public ActivityScenarioRule<TestActivity> mActivityScenarioRule =
            new ActivityScenarioRule<>(TestActivity.class);

    @Mock private Tab mTab;
    @Mock private Supplier<Tab> mTabSupplier;

    @Mock private ShareDelegate mShareDelegate;
    @Mock private Supplier<ShareDelegate> mShareDelegateSupplier;

    @Mock private PageInsightsCoordinator mPageInsightsCoordinator;
    @Mock private Supplier<PageInsightsCoordinator> mPageInsightsCoordinatorSupplier;

    private Activity mActivity;
    private GoogleBottomBarActionsHandler mGoogleBottomBarActionsHandler;

    @Before
    public void setup() {
        mActivityScenarioRule.getScenario().onActivity(activity -> mActivity = activity);
        MockitoAnnotations.initMocks(this);
        mGoogleBottomBarActionsHandler =
                new GoogleBottomBarActionsHandler(
                        mActivity,
                        mTabSupplier,
                        mShareDelegateSupplier,
                        mPageInsightsCoordinatorSupplier);

        when(mTabSupplier.get()).thenReturn(mTab);
        when(mTab.getUrl()).thenReturn(mGURL);
        when(mShareDelegateSupplier.get()).thenReturn(mShareDelegate);
    }

    @Test
    public void testSaveAction_buttonConfigHasPendingIntent_startsPendingIntent()
            throws PendingIntent.CanceledException {
        HistogramWatcher histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "CustomTabs.GoogleBottomBar.ButtonClicked",
                        GoogleBottomBarButtonEvent.SAVE_EMBEDDER);
        PendingIntent pendingIntent = mock(PendingIntent.class);
        Context context = mActivity.getApplicationContext();
        BottomBarConfig.ButtonConfig buttonConfig =
                new BottomBarConfig.ButtonConfig(
                        BottomBarConfigCreator.ButtonId.SAVE,
                        context.getDrawable(R.drawable.bookmark),
                        "Save button",
                        /* pendingIntent= */ pendingIntent);

        View.OnClickListener clickListener =
                mGoogleBottomBarActionsHandler.getClickListener(buttonConfig);
        clickListener.onClick(new View(context));

        ArgumentCaptor<Intent> captor = ArgumentCaptor.forClass(Intent.class);
        verify(pendingIntent)
                .send(eq(mActivity), anyInt(), captor.capture(), any(), any(), any(), any());
        assertEquals(Uri.parse(TEST_URI), captor.getValue().getData());
        histogramWatcher.assertExpected();
    }

    @Test
    public void testSaveAction_buttonConfigHasNoPendingIntent_showsTooltip() {
        HistogramWatcher histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "CustomTabs.GoogleBottomBar.ButtonClicked",
                        GoogleBottomBarButtonEvent.SAVE_DISABLED);
        Context context = mActivity;
        View buttonView = new View(context);
        BottomBarConfig.ButtonConfig buttonConfig =
                new BottomBarConfig.ButtonConfig(
                        BottomBarConfigCreator.ButtonId.SAVE,
                        context.getDrawable(R.drawable.bookmark),
                        context.getString(
                                R.string.google_bottom_bar_save_disabled_button_description),
                        /* pendingIntent= */ null);
        TextBubble.setSkipShowCheckForTesting(true);

        View.OnClickListener clickListener =
                mGoogleBottomBarActionsHandler.getClickListener(buttonConfig);
        clickListener.onClick(buttonView);

        Set<TextBubble> textBubbleSet = TextBubble.getTextBubbleSetForTesting();
        assertEquals(1, textBubbleSet.size());
        histogramWatcher.assertExpected();
    }

    @Test
    public void testShareAction_buttonConfigHasPendingIntent_startsPendingIntent()
            throws PendingIntent.CanceledException {
        HistogramWatcher histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "CustomTabs.GoogleBottomBar.ButtonClicked",
                        GoogleBottomBarButtonEvent.SHARE_EMBEDDER);
        PendingIntent pendingIntent = mock(PendingIntent.class);
        Context context = mActivity.getApplicationContext();
        View buttonView = new View(context);
        BottomBarConfig.ButtonConfig buttonConfig =
                new BottomBarConfig.ButtonConfig(
                        BottomBarConfigCreator.ButtonId.SHARE,
                        context.getDrawable(R.drawable.ic_share_white_24dp),
                        context.getString(R.string.google_bottom_bar_share_button_description),
                        pendingIntent);

        View.OnClickListener clickListener =
                mGoogleBottomBarActionsHandler.getClickListener(buttonConfig);
        clickListener.onClick(buttonView);

        ArgumentCaptor<Intent> captor = ArgumentCaptor.forClass(Intent.class);
        verify(pendingIntent)
                .send(eq(mActivity), anyInt(), captor.capture(), any(), any(), any(), any());
        assertEquals(Uri.parse(TEST_URI), captor.getValue().getData());
        histogramWatcher.assertExpected();
    }

    @Test
    public void testShareAction_initiateShareForCurrentTab() {
        HistogramWatcher histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "CustomTabs.GoogleBottomBar.ButtonClicked",
                        GoogleBottomBarButtonEvent.SHARE_CHROME);
        Context context = mActivity.getApplicationContext();
        View buttonView = new View(context);
        BottomBarConfig.ButtonConfig buttonConfig =
                new BottomBarConfig.ButtonConfig(
                        BottomBarConfigCreator.ButtonId.SHARE,
                        context.getDrawable(R.drawable.ic_share_white_24dp),
                        context.getString(R.string.google_bottom_bar_share_button_description),
                        /* pendingIntent= */ null);

        View.OnClickListener clickListener =
                mGoogleBottomBarActionsHandler.getClickListener(buttonConfig);
        clickListener.onClick(buttonView);

        verify(mShareDelegate)
                .share(eq(mTab), eq(false), eq(ShareDelegate.ShareOrigin.GOOGLE_BOTTOM_BAR));
        histogramWatcher.assertExpected();
    }

    @Test
    public void
            testPageInsightsAction_pageInsightCoordinatorNotNull_initiatePageInsightsCoordinatorLaunch() {
        HistogramWatcher histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "CustomTabs.GoogleBottomBar.ButtonClicked",
                        GoogleBottomBarButtonEvent.PIH_CHROME);
        when(mPageInsightsCoordinatorSupplier.get()).thenReturn(mPageInsightsCoordinator);
        Context context = mActivity.getApplicationContext();
        View buttonView = new View(context);
        BottomBarConfig.ButtonConfig buttonConfig =
                new BottomBarConfig.ButtonConfig(
                        BottomBarConfigCreator.ButtonId.PIH_BASIC,
                        context.getDrawable(R.drawable.page_insights_icon),
                        context.getString(
                                R.string.google_bottom_bar_page_insights_button_description),
                        /* pendingIntent= */ null);

        View.OnClickListener clickListener =
                mGoogleBottomBarActionsHandler.getClickListener(buttonConfig);
        clickListener.onClick(buttonView);

        verify(mPageInsightsCoordinator).launch();
        histogramWatcher.assertExpected();
    }

    @Test
    public void testPageInsightsAction_buttonConfigHasPendingIntent_startsPendingIntent()
            throws PendingIntent.CanceledException {
        HistogramWatcher histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "CustomTabs.GoogleBottomBar.ButtonClicked",
                        GoogleBottomBarButtonEvent.PIH_EMBEDDER);
        PendingIntent pendingIntent = mock(PendingIntent.class);
        Context context = mActivity.getApplicationContext();
        View buttonView = new View(context);
        BottomBarConfig.ButtonConfig buttonConfig =
                new BottomBarConfig.ButtonConfig(
                        BottomBarConfigCreator.ButtonId.PIH_BASIC,
                        context.getDrawable(R.drawable.page_insights_icon),
                        context.getString(
                                R.string.google_bottom_bar_page_insights_button_description),
                        pendingIntent);

        View.OnClickListener clickListener =
                mGoogleBottomBarActionsHandler.getClickListener(buttonConfig);
        clickListener.onClick(buttonView);

        ArgumentCaptor<Intent> captor = ArgumentCaptor.forClass(Intent.class);
        verify(pendingIntent)
                .send(eq(mActivity), anyInt(), captor.capture(), any(), any(), any(), any());
        assertEquals(Uri.parse(TEST_URI), captor.getValue().getData());
        histogramWatcher.assertExpected();
    }

    @Test
    public void testPageInsightsAction_buttonConfigHasNoPendingIntent_logsError() {
        HistogramWatcher histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectNoRecords("CustomTabs.GoogleBottomBar.ButtonClicked")
                        .build();
        Context context = mActivity.getApplicationContext();
        View buttonView = new View(context);
        BottomBarConfig.ButtonConfig buttonConfig =
                new BottomBarConfig.ButtonConfig(
                        BottomBarConfigCreator.ButtonId.PIH_BASIC,
                        context.getDrawable(R.drawable.page_insights_icon),
                        context.getString(
                                R.string.google_bottom_bar_page_insights_button_description),
                        /* pendingIntent= */ null);

        View.OnClickListener clickListener =
                mGoogleBottomBarActionsHandler.getClickListener(buttonConfig);
        clickListener.onClick(buttonView);

        ShadowLog.LogItem logItem = ShadowLog.getLogsForTag("cr_GBBActionHandler").get(0);
        assertEquals(logItem.msg, "Can't perform page insights action as pending intent is null.");
        histogramWatcher.assertExpected();
    }
}
