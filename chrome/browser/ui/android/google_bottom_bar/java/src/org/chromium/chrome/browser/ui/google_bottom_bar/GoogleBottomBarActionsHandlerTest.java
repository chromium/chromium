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

import org.chromium.base.supplier.Supplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.share.ShareDelegate;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.components.browser_ui.widget.textbubble.TextBubble;
import org.chromium.ui.base.TestActivity;
import org.chromium.url.GURL;

import java.util.Set;

/** Unit tests for {@link BottomBarConfig}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
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

    private Activity mActivity;
    private GoogleBottomBarActionsHandler mGoogleBottomBarActionsHandler;

    @Before
    public void setup() {
        mActivityScenarioRule.getScenario().onActivity(activity -> mActivity = activity);
        MockitoAnnotations.initMocks(this);
        mGoogleBottomBarActionsHandler =
                new GoogleBottomBarActionsHandler(mActivity, mTabSupplier, mShareDelegateSupplier);

        when(mTabSupplier.get()).thenReturn(mTab);
        when(mTab.getUrl()).thenReturn(mGURL);
        when(mShareDelegateSupplier.get()).thenReturn(mShareDelegate);
    }

    @Test
    public void testSaveAction_buttonConfigHasPendingIntent_startsPendingIntent()
            throws PendingIntent.CanceledException {
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
    }

    @Test
    public void testSaveAction_buttonConfigHasNoPendingIntent_showsTooltip() {
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
    }

    @Test
    public void testShareAction_initiateShareForCurrentTab() {
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
    }
}
