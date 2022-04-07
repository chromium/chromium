// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.messages;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.matcher.ViewMatchers.withChild;
import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import android.app.Activity;
import android.view.LayoutInflater;
import android.view.ViewGroup;
import android.widget.FrameLayout;

import androidx.test.filters.MediumTest;

import org.junit.Before;
import org.junit.BeforeClass;
import org.junit.ClassRule;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseActivityTestRule;
import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.components.browser_ui.widget.listmenu.BasicListMenu;
import org.chromium.components.browser_ui.widget.listmenu.ListMenu;
import org.chromium.components.browser_ui.widget.listmenu.ListMenuButton.PopupMenuShownListener;
import org.chromium.components.browser_ui.widget.listmenu.ListMenuButtonDelegate;
import org.chromium.components.browser_ui.widget.listmenu.ListMenuItemProperties;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.modelutil.MVCListAdapter;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;
import org.chromium.ui.test.util.BlankUiTestActivity;
import org.chromium.ui.test.util.DisableAnimationsTestRule;

/**
 * Instrumentation tests for MessageBannerView.
 */
@RunWith(BaseJUnit4ClassRunner.class)
@Batch(Batch.UNIT_TESTS)
public class MessageBannerViewTest {
    private static final String SECONDARY_BUTTON_MENU_TEXT = "SecondaryActionText";

    @ClassRule
    public static DisableAnimationsTestRule sDisableAnimationsRule =
            new DisableAnimationsTestRule();

    @ClassRule
    public static BaseActivityTestRule<BlankUiTestActivity> sActivityTestRule =
            new BaseActivityTestRule<>(BlankUiTestActivity.class);

    private static Activity sActivity;
    private static ViewGroup sContentView;

    @Rule
    public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock
    Runnable mSecondaryActionCallback;

    MessageBannerView mMessageBannerView;

    @BeforeClass
    public static void setupSuite() {
        sActivityTestRule.launchActivity(null);
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            sActivity = sActivityTestRule.getActivity();
            sContentView = new FrameLayout(sActivity);
            sActivity.setContentView(sContentView);
        });
    }

    @Before
    public void setupTest() throws Exception {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            sContentView.removeAllViews();
            mMessageBannerView = (MessageBannerView) LayoutInflater.from(sActivity).inflate(
                    R.layout.message_banner_view, sContentView, false);
            sContentView.addView(mMessageBannerView);
        });
    }

    /**
     * Tests that, when SECONDARY_BUTTON_MENU_TEXT is not specified, clicking on secondary button
     * triggers ON_SECONDARY_ACTION callback invocation.
     */
    @Test
    @MediumTest
    public void testSecondaryActionDirectCallback() {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            PropertyModel propertyModel =
                    new PropertyModel.Builder(MessageBannerProperties.ALL_KEYS)
                            .with(MessageBannerProperties.MESSAGE_IDENTIFIER,
                                    MessageIdentifier.TEST_MESSAGE)
                            .with(MessageBannerProperties.SECONDARY_ICON_RESOURCE_ID,
                                    android.R.drawable.ic_menu_add)
                            .with(MessageBannerProperties.ON_SECONDARY_BUTTON_CLICK,
                                    mSecondaryActionCallback)
                            .build();
            PropertyModelChangeProcessor.create(
                    propertyModel, mMessageBannerView, MessageBannerViewBinder::bind);
        });
        onView(withId(R.id.message_secondary_button)).perform(click());
        Mockito.verify(mSecondaryActionCallback).run();
    }

    /**
     * Tests that clicking on secondary button opens a menu with an item with
     * SECONDARY_BUTTON_MENU_TEXT. Clicking on this item triggers ON_SECONDARY_ACTION callback
     * invocation.
     */
    @Test
    @MediumTest
    public void testSecondaryActionMenu() {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            PropertyModel propertyModel =
                    new PropertyModel.Builder(MessageBannerProperties.ALL_KEYS)
                            .with(MessageBannerProperties.MESSAGE_IDENTIFIER,
                                    MessageIdentifier.TEST_MESSAGE)
                            .with(MessageBannerProperties.SECONDARY_ICON_RESOURCE_ID,
                                    android.R.drawable.ic_menu_add)
                            .with(MessageBannerProperties.SECONDARY_BUTTON_MENU_TEXT,
                                    SECONDARY_BUTTON_MENU_TEXT)
                            .with(MessageBannerProperties.ON_SECONDARY_BUTTON_CLICK,
                                    mSecondaryActionCallback)
                            .build();
            PropertyModelChangeProcessor.create(
                    propertyModel, mMessageBannerView, MessageBannerViewBinder::bind);
        });
        onView(withId(R.id.message_secondary_button)).perform(click());
        onView(withText(SECONDARY_BUTTON_MENU_TEXT)).perform(click());
        Mockito.verify(mSecondaryActionCallback).run();
    }

    /**
     * Tests that clicking on the secondary button opens a menu with an item with
     * SECONDARY_BUTTON_MENU_TEXT, the message auto-dismiss timer is cancelled at this time and the
     * message banner is shown as long as the menu is open. When the menu is dismissed, the message
     * auto-dismiss timer is (re)started with expected parameters.
     */
    @Test
    @MediumTest
    public void testSecondaryActionMenuInvokesPopupMenuEventHandlers() {
        PopupMenuShownListener listener = Mockito.spy(PopupMenuShownListener.class);
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            PropertyModel propertyModel =
                    new PropertyModel.Builder(MessageBannerProperties.ALL_KEYS)
                            .with(MessageBannerProperties.MESSAGE_IDENTIFIER,
                                    MessageIdentifier.TEST_MESSAGE)
                            .with(MessageBannerProperties.SECONDARY_ICON_RESOURCE_ID,
                                    android.R.drawable.ic_menu_add)
                            .with(MessageBannerProperties.SECONDARY_BUTTON_MENU_TEXT,
                                    SECONDARY_BUTTON_MENU_TEXT)
                            .with(MessageBannerProperties.ON_SECONDARY_BUTTON_CLICK,
                                    mSecondaryActionCallback)
                            .build();
            PropertyModelChangeProcessor.create(
                    propertyModel, mMessageBannerView, MessageBannerViewBinder::bind);
            // Simulate the invocation of #setPopupMenuShownListener by the MessageBannerCoordinator
            // ctor.
            mMessageBannerView.setPopupMenuShownListener(listener);
        });

        // Click on the secondary icon to open the popup menu, verify that #onPopupMenuShown is
        // invoked.
        onView(withId(R.id.message_secondary_button)).perform(click());
        Mockito.verify(listener).onPopupMenuShown();

        // Click on the message banner view holding the popup menu to dismiss the menu, verify that
        // #onPopupMenuDismissed is invoked.
        onView(withChild(withText(SECONDARY_BUTTON_MENU_TEXT))).perform(click());
        Mockito.verify(listener).onPopupMenuDismissed();
    }

    /**
     * Tests that clicking on secondary button opens a menu determined by
     * SECONDARY_MENU_BUTTON_DELEGATE.
     */
    @Test
    @MediumTest
    public void testSecondaryActionMenuWithCustomDelegate() {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            MVCListAdapter.ModelList menuItems = new MVCListAdapter.ModelList();
            menuItems.add(new MVCListAdapter.ListItem(BasicListMenu.ListMenuItemType.MENU_ITEM,
                    new PropertyModel.Builder(ListMenuItemProperties.ALL_KEYS)
                            .with(ListMenuItemProperties.TITLE, SECONDARY_BUTTON_MENU_TEXT)
                            .with(ListMenuItemProperties.ENABLED, true)
                            .build()));

            BasicListMenu listMenu =
                    new BasicListMenu(sActivity, menuItems, (PropertyModel menuItem) -> {
                        assert menuItem == menuItems.get(0).model;
                        mSecondaryActionCallback.run();
                    });

            PropertyModel propertyModel =
                    new PropertyModel.Builder(MessageBannerProperties.ALL_KEYS)
                            .with(MessageBannerProperties.MESSAGE_IDENTIFIER,
                                    MessageIdentifier.TEST_MESSAGE)
                            .with(MessageBannerProperties.SECONDARY_ICON_RESOURCE_ID,
                                    android.R.drawable.ic_menu_add)
                            .with(MessageBannerProperties.SECONDARY_MENU_BUTTON_DELEGATE,
                                    new ListMenuButtonDelegate() {
                                        @Override
                                        public ListMenu getListMenu() {
                                            return listMenu;
                                        }
                                    })
                            .build();
            PropertyModelChangeProcessor.create(
                    propertyModel, mMessageBannerView, MessageBannerViewBinder::bind);
        });
        onView(withId(R.id.message_secondary_button)).perform(click());
        onView(withText(SECONDARY_BUTTON_MENU_TEXT)).perform(click());
        Mockito.verify(mSecondaryActionCallback).run();
    }
}
