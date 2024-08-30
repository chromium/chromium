// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.google_bottom_bar;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import static org.chromium.chrome.browser.gsa.GSAUtils.GSA_CLASS_NAME;
import static org.chromium.chrome.browser.gsa.GSAUtils.GSA_PACKAGE_NAME;
import static org.chromium.chrome.browser.gsa.GSAUtils.VOICE_SEARCH_INTENT_ACTION;
import static org.chromium.chrome.browser.ui.google_bottom_bar.BottomBarConfig.ButtonId.PIH_BASIC;
import static org.chromium.chrome.browser.ui.google_bottom_bar.GoogleBottomBarActionsHandler.EXTRA_IS_LAUNCHED_FROM_CHROME_SEARCH_ENTRYPOINT;
import static org.chromium.chrome.browser.ui.google_bottom_bar.GoogleBottomBarLogger.BUTTON_CLICKED_HISTOGRAM;

import android.app.Activity;
import android.app.PendingIntent;
import android.app.SearchManager;
import android.content.Context;
import android.content.Intent;
import android.content.pm.ResolveInfo;
import android.graphics.drawable.Drawable;
import android.net.Uri;
import android.view.View;

import androidx.test.ext.junit.rules.ActivityScenarioRule;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.Shadows;
import org.robolectric.annotation.Config;
import org.robolectric.annotation.Implements;
import org.robolectric.annotation.Resetter;
import org.robolectric.shadows.ShadowLog;
import org.robolectric.shadows.ShadowPackageManager;

import org.chromium.base.supplier.Supplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.chrome.browser.lens.LensController;
import org.chromium.chrome.browser.lens.LensEntryPoint;
import org.chromium.chrome.browser.lens.LensIntentParams;
import org.chromium.chrome.browser.share.ShareDelegate;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.ui.google_bottom_bar.BottomBarConfig.ButtonId;
import org.chromium.chrome.browser.ui.google_bottom_bar.GoogleBottomBarLogger.GoogleBottomBarButtonEvent;
import org.chromium.components.browser_ui.widget.textbubble.TextBubble;
import org.chromium.ui.base.TestActivity;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.url.GURL;

import java.lang.ref.WeakReference;
import java.util.Set;

/** Unit tests for {@link BottomBarConfig}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(
        manifest = Config.NONE,
        shadows = {ShadowLog.class, GoogleBottomBarActionsHandlerTest.ShadowLensController.class})
public class GoogleBottomBarActionsHandlerTest {
    private static final String TEST_URI = "https://www.test.com/";

    private final GURL mGURL = new GURL(TEST_URI);

    @Implements(LensController.class)
    public static class ShadowLensController {
        public static boolean sIsAvailable;

        public static LensController sController;

        public static LensController getInstance() {
            if (sController == null) {
                sController = mock(LensController.class);
            }
            doReturn(sIsAvailable).when(sController).isLensEnabled(any());
            return sController;
        }

        @Resetter
        public static void reset() {
            sIsAvailable = false;
            sController = null;
        }
    }

    @Rule
    public ActivityScenarioRule<TestActivity> mActivityScenarioRule =
            new ActivityScenarioRule<>(TestActivity.class);

    @Mock private WindowAndroid mWindowAndroid;
    @Mock private Tab mTab;
    @Mock private Supplier<Tab> mTabSupplier;

    @Mock private ShareDelegate mShareDelegate;
    @Mock private Supplier<ShareDelegate> mShareDelegateSupplier;

    @Captor private ArgumentCaptor<LensIntentParams> mLensIntentParamsArgumentCaptor;

    private Activity mActivity;
    private GoogleBottomBarActionsHandler mGoogleBottomBarActionsHandler;
    private HistogramWatcher mHistogramWatcher;
    private ShadowPackageManager mShadowPackageManager;

    @Before
    public void setup() {
        mActivityScenarioRule.getScenario().onActivity(activity -> mActivity = activity);
        MockitoAnnotations.initMocks(this);
        mGoogleBottomBarActionsHandler =
                new GoogleBottomBarActionsHandler(mActivity, mTabSupplier, mShareDelegateSupplier);

        mShadowPackageManager =
                Shadows.shadowOf(RuntimeEnvironment.application.getPackageManager());

        when(mTabSupplier.get()).thenReturn(mTab);
        when(mTab.getUrl()).thenReturn(mGURL);
        when(mTab.getWindowAndroid()).thenReturn(mWindowAndroid);
        when(mWindowAndroid.getContext()).thenReturn(new WeakReference<>(mActivity));
        when(mShareDelegateSupplier.get()).thenReturn(mShareDelegate);
    }

    @After
    public void tearDown() {
        ShadowLensController.reset();
        if (mHistogramWatcher != null) {
            mHistogramWatcher.assertExpected();
            mHistogramWatcher.close();
            mHistogramWatcher = null;
        }
    }

    @Test
    public void testSaveAction_buttonConfigHasPendingIntent_startsPendingIntent()
            throws PendingIntent.CanceledException {
        mHistogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        BUTTON_CLICKED_HISTOGRAM, GoogleBottomBarButtonEvent.SAVE_EMBEDDER);
        PendingIntent pendingIntent = mock(PendingIntent.class);
        Context context = mActivity.getApplicationContext();
        BottomBarConfig.ButtonConfig buttonConfig =
                new BottomBarConfig.ButtonConfig(
                        ButtonId.SAVE,
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
    }

    @Test
    public void testSaveAction_buttonConfigHasNoPendingIntent_showsTooltip() {
        mHistogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        BUTTON_CLICKED_HISTOGRAM, GoogleBottomBarButtonEvent.SAVE_DISABLED);
        Context context = mActivity;
        View buttonView = new View(context);
        BottomBarConfig.ButtonConfig buttonConfig =
                new BottomBarConfig.ButtonConfig(
                        ButtonId.SAVE,
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
    }

    @Test
    public void testShareAction_buttonConfigHasPendingIntent_startsPendingIntent()
            throws PendingIntent.CanceledException {
        mHistogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        BUTTON_CLICKED_HISTOGRAM, GoogleBottomBarButtonEvent.SHARE_EMBEDDER);
        PendingIntent pendingIntent = mock(PendingIntent.class);
        Context context = mActivity.getApplicationContext();
        View buttonView = new View(context);
        BottomBarConfig.ButtonConfig buttonConfig =
                new BottomBarConfig.ButtonConfig(
                        ButtonId.SHARE,
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
    }

    @Test
    public void testShareAction_initiateShareForCurrentTab() {
        mHistogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        BUTTON_CLICKED_HISTOGRAM, GoogleBottomBarButtonEvent.SHARE_CHROME);
        Context context = mActivity.getApplicationContext();
        View buttonView = new View(context);
        BottomBarConfig.ButtonConfig buttonConfig =
                new BottomBarConfig.ButtonConfig(
                        ButtonId.SHARE,
                        context.getDrawable(R.drawable.ic_share_white_24dp),
                        context.getString(R.string.google_bottom_bar_share_button_description),
                        /* pendingIntent= */ null);

        View.OnClickListener clickListener =
                mGoogleBottomBarActionsHandler.getClickListener(buttonConfig);
        clickListener.onClick(buttonView);

        verify(mShareDelegate)
                .share(eq(mTab), eq(false), eq(ShareDelegate.ShareOrigin.GOOGLE_BOTTOM_BAR));
    }

    @Test
    public void testPageInsightsAction_buttonConfigHasPendingIntent_startsPendingIntent()
            throws PendingIntent.CanceledException {
        mHistogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        BUTTON_CLICKED_HISTOGRAM, GoogleBottomBarButtonEvent.PIH_EMBEDDER);
        PendingIntent pendingIntent = mock(PendingIntent.class);
        Context context = mActivity.getApplicationContext();
        View buttonView = new View(context);
        BottomBarConfig.ButtonConfig buttonConfig =
                new BottomBarConfig.ButtonConfig(
                        PIH_BASIC,
                        context.getDrawable(R.drawable.bottom_bar_page_insights_icon),
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
    }

    @Test
    public void testPageInsightsAction_buttonConfigHasNoPendingIntent_logsError() {
        mHistogramWatcher =
                HistogramWatcher.newBuilder().expectNoRecords(BUTTON_CLICKED_HISTOGRAM).build();
        Context context = mActivity.getApplicationContext();
        View buttonView = new View(context);
        BottomBarConfig.ButtonConfig buttonConfig =
                new BottomBarConfig.ButtonConfig(
                        PIH_BASIC,
                        context.getDrawable(R.drawable.bottom_bar_page_insights_icon),
                        context.getString(
                                R.string.google_bottom_bar_page_insights_button_description),
                        /* pendingIntent= */ null);

        View.OnClickListener clickListener =
                mGoogleBottomBarActionsHandler.getClickListener(buttonConfig);
        clickListener.onClick(buttonView);

        ShadowLog.LogItem logItem = ShadowLog.getLogsForTag("cr_GBBActionHandler").get(0);
        assertEquals(logItem.msg, "Can't perform action with id: 1 as pending intent is null.");
    }

    @Test
    public void testCustomAction_buttonConfigHasNoPendingIntent_logsError() {
        mHistogramWatcher =
                HistogramWatcher.newBuilder().expectNoRecords(BUTTON_CLICKED_HISTOGRAM).build();
        Context context = mActivity.getApplicationContext();
        View buttonView = new View(context);
        Drawable icon = mock(Drawable.class);
        BottomBarConfig.ButtonConfig buttonConfig =
                new BottomBarConfig.ButtonConfig(
                        ButtonId.CUSTOM,
                        icon,
                        /* description= */ "Description",
                        /* pendingIntent= */ null);

        View.OnClickListener clickListener =
                mGoogleBottomBarActionsHandler.getClickListener(buttonConfig);
        clickListener.onClick(buttonView);

        ShadowLog.LogItem logItem = ShadowLog.getLogsForTag("cr_GBBActionHandler").get(0);
        assertEquals(logItem.msg, "Can't perform action with id: 8 as pending intent is null.");
    }

    @Test
    public void testCustomAction_buttonConfigHasPendingIntent_startsPendingIntent()
            throws PendingIntent.CanceledException {
        mHistogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        BUTTON_CLICKED_HISTOGRAM, GoogleBottomBarButtonEvent.CUSTOM_EMBEDDER);
        Context context = mActivity.getApplicationContext();
        View buttonView = new View(context);
        PendingIntent pendingIntent = mock(PendingIntent.class);
        Drawable icon = mock(Drawable.class);
        BottomBarConfig.ButtonConfig buttonConfig =
                new BottomBarConfig.ButtonConfig(
                        ButtonId.CUSTOM,
                        icon,
                        /* description= */ "Description",
                        /* pendingIntent= */ pendingIntent);

        View.OnClickListener clickListener =
                mGoogleBottomBarActionsHandler.getClickListener(buttonConfig);
        clickListener.onClick(buttonView);

        ArgumentCaptor<Intent> captor = ArgumentCaptor.forClass(Intent.class);
        verify(pendingIntent)
                .send(eq(mActivity), anyInt(), captor.capture(), any(), any(), any(), any());
        assertEquals(Uri.parse(TEST_URI), captor.getValue().getData());
    }

    @Test(expected = IllegalStateException.class)
    public void
            testSearchAction_buttonConfigHasNoPendingIntent_canNotBeResolved_throwsIllegalStateException() {
        mHistogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        BUTTON_CLICKED_HISTOGRAM, GoogleBottomBarButtonEvent.SEARCH_CHROME);
        Context context = mActivity.getApplicationContext();
        View buttonView = new View(context);
        Drawable icon = mock(Drawable.class);
        BottomBarConfig.ButtonConfig buttonConfig =
                new BottomBarConfig.ButtonConfig(
                        ButtonId.SEARCH,
                        icon,
                        /* description= */ "Description",
                        /* pendingIntent= */ null);

        View.OnClickListener clickListener =
                mGoogleBottomBarActionsHandler.getClickListener(buttonConfig);
        clickListener.onClick(buttonView);

        assertNull(Shadows.shadowOf(mActivity).getNextStartedActivityForResult());
        ShadowLog.LogItem logItem = ShadowLog.getLogsForTag("cr_GBBActionHandler").get(0);
        assertEquals(logItem.msg, "Can't resolve activity for action: openGoogleAppSearch");
    }

    @Test
    public void
            testSearchAction_buttonConfigHasNoPendingIntent_canBeResolved_googleAppSearchIntentStarted() {
        mHistogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        BUTTON_CLICKED_HISTOGRAM, GoogleBottomBarButtonEvent.SEARCH_CHROME);
        Context context = mActivity.getApplicationContext();
        View buttonView = new View(context);
        Drawable icon = mock(Drawable.class);
        BottomBarConfig.ButtonConfig buttonConfig =
                new BottomBarConfig.ButtonConfig(
                        ButtonId.SEARCH,
                        icon,
                        /* description= */ "Description",
                        /* pendingIntent= */ null);
        Intent intent = new Intent(SearchManager.INTENT_ACTION_GLOBAL_SEARCH);
        intent.setPackage(GSA_PACKAGE_NAME);
        mShadowPackageManager.addResolveInfoForIntent(intent, new ResolveInfo());

        View.OnClickListener clickListener =
                mGoogleBottomBarActionsHandler.getClickListener(buttonConfig);
        clickListener.onClick(buttonView);

        Intent startedIntent = Shadows.shadowOf(mActivity).getNextStartedActivityForResult().intent;
        assertEquals(SearchManager.INTENT_ACTION_GLOBAL_SEARCH, startedIntent.getAction());
        assertEquals(GSA_PACKAGE_NAME, startedIntent.getPackage());
        assertTrue(
                startedIntent
                        .getExtras()
                        .containsKey(EXTRA_IS_LAUNCHED_FROM_CHROME_SEARCH_ENTRYPOINT));
    }

    @Test
    public void testSearchAction_buttonConfigHasPendingIntent_startsPendingIntent()
            throws PendingIntent.CanceledException {
        mHistogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        BUTTON_CLICKED_HISTOGRAM, GoogleBottomBarButtonEvent.SEARCH_EMBEDDER);
        Context context = mActivity.getApplicationContext();
        View buttonView = new View(context);
        PendingIntent pendingIntent = mock(PendingIntent.class);
        Drawable icon = mock(Drawable.class);
        BottomBarConfig.ButtonConfig buttonConfig =
                new BottomBarConfig.ButtonConfig(
                        ButtonId.SEARCH,
                        icon,
                        /* description= */ "Description",
                        /* pendingIntent= */ pendingIntent);

        View.OnClickListener clickListener =
                mGoogleBottomBarActionsHandler.getClickListener(buttonConfig);
        clickListener.onClick(buttonView);

        ArgumentCaptor<Intent> captor = ArgumentCaptor.forClass(Intent.class);
        verify(pendingIntent)
                .send(eq(mActivity), anyInt(), captor.capture(), any(), any(), any(), any());
        assertEquals(Uri.parse(TEST_URI), captor.getValue().getData());
    }

    @Test(expected = IllegalStateException.class)
    public void
            testHomeAction_buttonConfigHasNoPendingIntent_canNotBeResolved_throwsIllegalStateException() {
        mHistogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        BUTTON_CLICKED_HISTOGRAM, GoogleBottomBarButtonEvent.HOME_CHROME);
        Context context = mActivity.getApplicationContext();
        View buttonView = new View(context);
        Drawable icon = mock(Drawable.class);
        BottomBarConfig.ButtonConfig buttonConfig =
                new BottomBarConfig.ButtonConfig(
                        ButtonId.HOME,
                        icon,
                        /* description= */ "Description",
                        /* pendingIntent= */ null);

        View.OnClickListener clickListener =
                mGoogleBottomBarActionsHandler.getClickListener(buttonConfig);
        clickListener.onClick(buttonView);

        ShadowLog.LogItem logItem = ShadowLog.getLogsForTag("cr_GBBActionHandler").get(0);
        assertEquals(logItem.msg, "Can't resolve activity for action: openGoogleAppHome");
    }

    @Test
    public void
            testHomeAction_buttonConfigHasNoPendingIntent_canBeResolved_googleAppHomeIntentStarted() {
        mHistogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        BUTTON_CLICKED_HISTOGRAM, GoogleBottomBarButtonEvent.HOME_CHROME);
        Context context = mActivity.getApplicationContext();
        View buttonView = new View(context);
        Drawable icon = mock(Drawable.class);
        BottomBarConfig.ButtonConfig buttonConfig =
                new BottomBarConfig.ButtonConfig(
                        ButtonId.HOME,
                        icon,
                        /* description= */ "Description",
                        /* pendingIntent= */ null);
        Intent intent = new Intent(Intent.ACTION_MAIN);
        intent.addCategory(Intent.CATEGORY_INFO);
        intent.setClassName(GSA_PACKAGE_NAME, GSA_CLASS_NAME);
        mShadowPackageManager.addResolveInfoForIntent(intent, new ResolveInfo());

        View.OnClickListener clickListener =
                mGoogleBottomBarActionsHandler.getClickListener(buttonConfig);
        clickListener.onClick(buttonView);

        Intent startedIntent = Shadows.shadowOf(mActivity).getNextStartedActivityForResult().intent;
        assertEquals(Intent.ACTION_MAIN, startedIntent.getAction());
        assertEquals(1, startedIntent.getCategories().size());
        assertTrue(startedIntent.getCategories().contains(Intent.CATEGORY_INFO));
        assertEquals(GSA_PACKAGE_NAME, startedIntent.getComponent().getPackageName());
        assertEquals(GSA_CLASS_NAME, startedIntent.getComponent().getShortClassName());
        assertTrue(
                startedIntent
                        .getExtras()
                        .containsKey(EXTRA_IS_LAUNCHED_FROM_CHROME_SEARCH_ENTRYPOINT));
    }

    @Test
    public void testHomeAction_buttonConfigHasPendingIntent_startsPendingIntent()
            throws PendingIntent.CanceledException {
        mHistogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        BUTTON_CLICKED_HISTOGRAM, GoogleBottomBarButtonEvent.HOME_EMBEDDER);
        Context context = mActivity.getApplicationContext();
        View buttonView = new View(context);
        PendingIntent pendingIntent = mock(PendingIntent.class);
        Drawable icon = mock(Drawable.class);
        BottomBarConfig.ButtonConfig buttonConfig =
                new BottomBarConfig.ButtonConfig(
                        ButtonId.HOME,
                        icon,
                        /* description= */ "Description",
                        /* pendingIntent= */ pendingIntent);

        View.OnClickListener clickListener =
                mGoogleBottomBarActionsHandler.getClickListener(buttonConfig);
        clickListener.onClick(buttonView);

        ArgumentCaptor<Intent> captor = ArgumentCaptor.forClass(Intent.class);
        verify(pendingIntent)
                .send(eq(mActivity), anyInt(), captor.capture(), any(), any(), any(), any());
        assertEquals(Uri.parse(TEST_URI), captor.getValue().getData());
    }

    @Test(expected = IllegalStateException.class)
    public void testOnSearchboxHomeTap_canNotBeResolved_throwsIllegalStateException() {
        mGoogleBottomBarActionsHandler.onSearchboxHomeTap();

        assertNull(Shadows.shadowOf(mActivity).getNextStartedActivityForResult());
        ShadowLog.LogItem logItem = ShadowLog.getLogsForTag("cr_GBBActionHandler").get(0);
        assertEquals(logItem.msg, "Can't resolve activity for action: openGoogleAppHome");
    }

    @Test
    public void testOnSearchboxHomeTap_canBeResolved_googleAppHomeIntentStarted() {
        mHistogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        BUTTON_CLICKED_HISTOGRAM, GoogleBottomBarButtonEvent.SEARCHBOX_HOME);
        Intent intent = new Intent(Intent.ACTION_MAIN);
        intent.addCategory(Intent.CATEGORY_INFO);
        intent.setClassName(GSA_PACKAGE_NAME, GSA_CLASS_NAME);
        mShadowPackageManager.addResolveInfoForIntent(intent, new ResolveInfo());

        mGoogleBottomBarActionsHandler.onSearchboxHomeTap();

        Intent startedIntent = Shadows.shadowOf(mActivity).getNextStartedActivityForResult().intent;
        assertEquals(Intent.ACTION_MAIN, startedIntent.getAction());
        assertEquals(1, startedIntent.getCategories().size());
        assertTrue(startedIntent.getCategories().contains(Intent.CATEGORY_INFO));
        assertEquals(GSA_PACKAGE_NAME, startedIntent.getComponent().getPackageName());
        assertEquals(GSA_CLASS_NAME, startedIntent.getComponent().getShortClassName());
        assertTrue(
                startedIntent
                        .getExtras()
                        .containsKey(EXTRA_IS_LAUNCHED_FROM_CHROME_SEARCH_ENTRYPOINT));
    }

    @Test(expected = IllegalStateException.class)
    public void testOnSearchboxHintTextTap_canNotBeResolved_throwsIllegalStateException() {
        mGoogleBottomBarActionsHandler.onSearchboxHintTextTap();

        assertNull(Shadows.shadowOf(mActivity).getNextStartedActivityForResult());
        ShadowLog.LogItem logItem = ShadowLog.getLogsForTag("cr_GBBActionHandler").get(0);
        assertEquals(logItem.msg, "Can't resolve activity for action: openGoogleAppSearch");
    }

    @Test
    public void testOnSearchboxHintTextTap_canBeResolved_googleAppSearchIntentStarted() {
        mHistogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        BUTTON_CLICKED_HISTOGRAM, GoogleBottomBarButtonEvent.SEARCHBOX_SEARCH);
        Intent intent = new Intent(SearchManager.INTENT_ACTION_GLOBAL_SEARCH);
        intent.setPackage(GSA_PACKAGE_NAME);
        mShadowPackageManager.addResolveInfoForIntent(intent, new ResolveInfo());

        mGoogleBottomBarActionsHandler.onSearchboxHintTextTap();

        Intent startedIntent = Shadows.shadowOf(mActivity).getNextStartedActivityForResult().intent;
        assertEquals(SearchManager.INTENT_ACTION_GLOBAL_SEARCH, startedIntent.getAction());
        assertEquals(GSA_PACKAGE_NAME, startedIntent.getPackage());
        assertTrue(
                startedIntent
                        .getExtras()
                        .containsKey(EXTRA_IS_LAUNCHED_FROM_CHROME_SEARCH_ENTRYPOINT));
    }

    @Test(expected = IllegalStateException.class)
    public void testOnSearchboxMicTap_canNotBeResolved_throwsIllegalStateException() {
        mGoogleBottomBarActionsHandler.onSearchboxMicTap();

        assertNull(Shadows.shadowOf(mActivity).getNextStartedActivityForResult());
        ShadowLog.LogItem logItem = ShadowLog.getLogsForTag("cr_GBBActionHandler").get(0);
        assertEquals(logItem.msg, "Can't resolve activity for action: openGoogleAppVoiceSearch");
    }

    @Test
    public void testOnSearchboxMicTap_canBeResolved_googleAppVoiceSearchIntentStarted() {
        mHistogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        BUTTON_CLICKED_HISTOGRAM,
                        GoogleBottomBarButtonEvent.SEARCHBOX_VOICE_SEARCH);
        Intent intent = new Intent(VOICE_SEARCH_INTENT_ACTION);
        intent.setPackage(GSA_PACKAGE_NAME);
        mShadowPackageManager.addResolveInfoForIntent(intent, new ResolveInfo());

        mGoogleBottomBarActionsHandler.onSearchboxMicTap();

        Intent startedIntent = Shadows.shadowOf(mActivity).getNextStartedActivityForResult().intent;
        assertEquals(VOICE_SEARCH_INTENT_ACTION, startedIntent.getAction());
        assertEquals(GSA_PACKAGE_NAME, startedIntent.getPackage());
        assertTrue(
                startedIntent
                        .getExtras()
                        .containsKey(EXTRA_IS_LAUNCHED_FROM_CHROME_SEARCH_ENTRYPOINT));
    }

    @Test
    public void testOnSearchboxLensTap_lensNotEnabled_lensNotStarted() {
        mHistogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        BUTTON_CLICKED_HISTOGRAM, GoogleBottomBarButtonEvent.SEARCHBOX_LENS);
        ShadowLensController.sIsAvailable = false;

        Context context = mActivity;
        mGoogleBottomBarActionsHandler.onSearchboxLensTap(new View(context));

        verify(ShadowLensController.getInstance(), never()).startLens(any(), any());
    }

    @Test
    public void testOnSearchboxLensTap_lensEnabled_lensStarted() {
        mHistogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        BUTTON_CLICKED_HISTOGRAM, GoogleBottomBarButtonEvent.SEARCHBOX_LENS);
        ShadowLensController.sIsAvailable = true;

        Context context = mActivity;
        mGoogleBottomBarActionsHandler.onSearchboxLensTap(new View(context));

        verify(ShadowLensController.getInstance())
                .startLens(any(), mLensIntentParamsArgumentCaptor.capture());
        LensIntentParams params = mLensIntentParamsArgumentCaptor.getValue();
        assertEquals(LensEntryPoint.GOOGLE_BOTTOM_BAR, params.getLensEntryPoint());
    }
}
