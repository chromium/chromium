// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.translate;

import static androidx.test.espresso.Espresso.onData;
import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.assertion.ViewAssertions.doesNotExist;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.ViewMatchers.hasDescendant;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.hamcrest.Matchers.allOf;
import static org.hamcrest.Matchers.instanceOf;
import static org.hamcrest.Matchers.not;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoMoreInteractions;

import android.app.Activity;
import android.view.View;
import android.view.ViewGroup;
import android.widget.FrameLayout;

import androidx.test.espresso.DataInteraction;
import androidx.test.filters.MediumTest;

import org.junit.Before;
import org.junit.BeforeClass;
import org.junit.ClassRule;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.BaseActivityTestRule;
import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.JniMocker;
import org.chromium.components.messages.MessageBannerProperties;
import org.chromium.components.messages.MessageDispatcher;
import org.chromium.components.messages.MessageScopeType;
import org.chromium.components.translate.TranslateMessage.MenuItem;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.listmenu.ListMenuButton;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.test.util.BlankUiTestActivity;

/** Instrumentation tests for the secondary menu functionality of TranslateMessage. */
@RunWith(BaseJUnit4ClassRunner.class)
@Batch(Batch.UNIT_TESTS)
public final class TranslateMessageSecondaryMenuTest {
    private static final long NATIVE_TRANSLATE_MESSAGE = 1337;
    private static final int DISMISSAL_DURATION_SECONDS = 10;
    private static final int LIST_MENU_BUTTON_ID = View.generateViewId();

    private static final MenuItem MENU_ITEM =
            new MenuItem(
                    /* title= */ "title of basic menu item",
                    /* subtitle= */ "",
                    /* hasCheckmark= */ false,
                    /* overflowMenuItemId= */ 0,
                    /* languageCode= */ "lang0");
    private static final MenuItem MENU_ITEM_WITH_SUBTITLE =
            new MenuItem(
                    /* title= */ "title of subtitled menu item",
                    /* subtitle= */ "menu item subtitle",
                    /* hasCheckmark= */ false,
                    /* overflowMenuItemId= */ 1,
                    /* languageCode= */ "lang1");
    private static final MenuItem MENU_ITEM_WITH_CHECKMARK =
            new MenuItem(
                    /* title= */ "title of checked menu item",
                    /* subtitle= */ "",
                    /* hasCheckmark= */ true,
                    /* overflowMenuItemId= */ 2,
                    /* languageCode= */ "lang2");
    private static final MenuItem MENU_ITEM_DIVIDER =
            new MenuItem(
                    /* title= */ "",
                    /* subtitle= */ "",
                    /* hasCheckmark= */ false,
                    /* overflowMenuItemId= */ 3,
                    /* languageCode= */ "lang3");

    @ClassRule
    public static BaseActivityTestRule<BlankUiTestActivity> sActivityTestRule =
            new BaseActivityTestRule<>(BlankUiTestActivity.class);

    private static Activity sActivity;
    private static ViewGroup sContentView;

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Rule public JniMocker mJniMocker = new JniMocker();

    @Mock TranslateMessage.Natives mMockJni;
    @Mock WebContents mWebContents;
    @Mock MessageDispatcher mMessageDispatcher;

    @Captor ArgumentCaptor<PropertyModel> mPropertyModelCaptor;

    @BeforeClass
    public static void setupSuite() {
        sActivityTestRule.launchActivity(null);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    sActivity = sActivityTestRule.getActivity();
                    sContentView = new FrameLayout(sActivity);
                    sActivity.setContentView(sContentView);
                });
    }

    @Before
    public void setupTest() throws Exception {
        mJniMocker.mock(TranslateMessageJni.TEST_HOOKS, mMockJni);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    sContentView.removeAllViews();
                });
    }

    @Test
    @MediumTest
    public void testShowMultipleMenuItems() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    prepareListMenuButtonForTranslateMessageOnUiThread();
                });

        doReturn(
                        new MenuItem[] {
                            MENU_ITEM,
                            MENU_ITEM_DIVIDER,
                            MENU_ITEM_WITH_SUBTITLE,
                            MENU_ITEM_WITH_CHECKMARK
                        })
                .when(mMockJni)
                .buildOverflowMenu(NATIVE_TRANSLATE_MESSAGE);
        onView(withId(LIST_MENU_BUTTON_ID)).perform(click());

        DataInteraction interaction = onData(instanceOf(MenuItem.class));

        // Basic titled menu item.
        interaction
                .atPosition(0)
                .check(
                        matches(
                                allOf(
                                        hasDescendant(
                                                allOf(
                                                        withId(R.id.menu_item_text),
                                                        withText(MENU_ITEM.title),
                                                        isDisplayed())),
                                        not(hasDescendant(withId(R.id.menu_item_secondary_text))),
                                        not(hasDescendant(withId(R.id.menu_item_icon))))));

        // Divider.
        interaction
                .atPosition(1)
                .check(
                        matches(
                                allOf(
                                        not(hasDescendant(withId(R.id.menu_item_text))),
                                        not(hasDescendant(withId(R.id.menu_item_secondary_text))),
                                        not(hasDescendant(withId(R.id.menu_item_icon))))));

        // Subtitled menu item.
        interaction
                .atPosition(2)
                .check(
                        matches(
                                allOf(
                                        hasDescendant(
                                                allOf(
                                                        withId(R.id.menu_item_text),
                                                        withText(MENU_ITEM_WITH_SUBTITLE.title),
                                                        isDisplayed())),
                                        hasDescendant(
                                                allOf(
                                                        withId(R.id.menu_item_secondary_text),
                                                        withText(MENU_ITEM_WITH_SUBTITLE.subtitle),
                                                        isDisplayed())),
                                        not(hasDescendant(withId(R.id.menu_item_icon))))));

        // Checkmarked menu item.
        interaction
                .atPosition(3)
                .check(
                        matches(
                                allOf(
                                        hasDescendant(
                                                allOf(
                                                        withId(R.id.menu_item_text),
                                                        withText(MENU_ITEM_WITH_CHECKMARK.title),
                                                        isDisplayed())),
                                        not(hasDescendant(withId(R.id.menu_item_secondary_text))),
                                        hasDescendant(
                                                allOf(
                                                        withId(R.id.menu_item_icon),
                                                        isDisplayed())))));
    }

    @Test
    @MediumTest
    public void testMenuItemViewReUse() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    prepareListMenuButtonForTranslateMessageOnUiThread();
                });

        // Use a very large list of MenuItems, such that views have an opportunity to be re-used.
        final int numBasicMenuItems = 100;
        MenuItem[] menuItems = new MenuItem[numBasicMenuItems + 1];
        for (int i = 0; i < numBasicMenuItems; ++i) {
            menuItems[i] =
                    new MenuItem(
                            /* title= */ "title " + i,
                            /* subtitle= */ "",
                            /* hasCheckmark= */ false,
                            /* overflowMenuItemId= */ 0,
                            /* languageCode= */ "");
        }
        // Add a subtitled MenuItem to the end, which should not re-use an earlier menu item view
        // since it should have a different view layout.
        menuItems[numBasicMenuItems] = MENU_ITEM_WITH_SUBTITLE;

        doReturn(menuItems).when(mMockJni).buildOverflowMenu(NATIVE_TRANSLATE_MESSAGE);
        onView(withId(LIST_MENU_BUTTON_ID)).perform(click());

        // The topmost menu item should be visible, but the bottommost two menu items should not be
        // visible on the screen yet.
        onView(withText(menuItems[0].title)).check(matches(isDisplayed()));
        onView(withText(menuItems[numBasicMenuItems - 1].title)).check(doesNotExist());
        onView(withText(MENU_ITEM_WITH_SUBTITLE.title)).check(doesNotExist());

        DataInteraction interaction = onData(instanceOf(MenuItem.class));

        for (int i = 0; i < numBasicMenuItems; ++i) {
            interaction
                    .atPosition(i)
                    .check(
                            matches(
                                    allOf(
                                            hasDescendant(
                                                    allOf(
                                                            withId(R.id.menu_item_text),
                                                            withText(menuItems[i].title),
                                                            isDisplayed())),
                                            not(
                                                    hasDescendant(
                                                            withId(R.id.menu_item_secondary_text))),
                                            not(hasDescendant(withId(R.id.menu_item_icon))))));
        }

        // Scroll to and verify that the subtitled menu item is displayed correctly.
        interaction
                .atPosition(numBasicMenuItems)
                .check(
                        matches(
                                allOf(
                                        hasDescendant(
                                                allOf(
                                                        withId(R.id.menu_item_text),
                                                        withText(MENU_ITEM_WITH_SUBTITLE.title),
                                                        isDisplayed())),
                                        hasDescendant(
                                                allOf(
                                                        withId(R.id.menu_item_secondary_text),
                                                        withText(MENU_ITEM_WITH_SUBTITLE.subtitle),
                                                        isDisplayed())),
                                        not(hasDescendant(withId(R.id.menu_item_icon))))));

        // The topmost view should have been scrolled off screen by this point.
        onView(withText(menuItems[0].title)).check(doesNotExist());
    }

    @Test
    @MediumTest
    public void testClickMenuItem() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    prepareListMenuButtonForTranslateMessageOnUiThread();
                });

        doReturn(new MenuItem[] {MENU_ITEM})
                .when(mMockJni)
                .buildOverflowMenu(NATIVE_TRANSLATE_MESSAGE);
        onView(withId(LIST_MENU_BUTTON_ID)).perform(click());

        doReturn(null)
                .when(mMockJni)
                .handleSecondaryMenuItemClicked(
                        NATIVE_TRANSLATE_MESSAGE,
                        MENU_ITEM.overflowMenuItemId,
                        MENU_ITEM.languageCode,
                        MENU_ITEM.hasCheckmark);
        onView(withText(MENU_ITEM.title)).perform(click());
        verify(mMockJni)
                .handleSecondaryMenuItemClicked(
                        NATIVE_TRANSLATE_MESSAGE,
                        MENU_ITEM.overflowMenuItemId,
                        MENU_ITEM.languageCode,
                        MENU_ITEM.hasCheckmark);

        onView(withText(MENU_ITEM.title)).check(doesNotExist());
    }

    @Test
    @MediumTest
    public void testClickMenuItemWithNestedMenu() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    prepareListMenuButtonForTranslateMessageOnUiThread();
                });

        doReturn(new MenuItem[] {MENU_ITEM})
                .when(mMockJni)
                .buildOverflowMenu(NATIVE_TRANSLATE_MESSAGE);
        onView(withId(LIST_MENU_BUTTON_ID)).perform(click());

        doReturn(new MenuItem[] {MENU_ITEM_WITH_SUBTITLE})
                .when(mMockJni)
                .handleSecondaryMenuItemClicked(
                        NATIVE_TRANSLATE_MESSAGE,
                        MENU_ITEM.overflowMenuItemId,
                        MENU_ITEM.languageCode,
                        MENU_ITEM.hasCheckmark);
        onView(withText(MENU_ITEM.title)).perform(click());
        verify(mMockJni)
                .handleSecondaryMenuItemClicked(
                        NATIVE_TRANSLATE_MESSAGE,
                        MENU_ITEM.overflowMenuItemId,
                        MENU_ITEM.languageCode,
                        MENU_ITEM.hasCheckmark);

        onView(withText(MENU_ITEM.title)).check(doesNotExist());

        doReturn(null)
                .when(mMockJni)
                .handleSecondaryMenuItemClicked(
                        NATIVE_TRANSLATE_MESSAGE,
                        MENU_ITEM_WITH_SUBTITLE.overflowMenuItemId,
                        MENU_ITEM_WITH_SUBTITLE.languageCode,
                        MENU_ITEM_WITH_SUBTITLE.hasCheckmark);
        onView(withText(MENU_ITEM_WITH_SUBTITLE.title)).perform(click());
        verify(mMockJni)
                .handleSecondaryMenuItemClicked(
                        NATIVE_TRANSLATE_MESSAGE,
                        MENU_ITEM_WITH_SUBTITLE.overflowMenuItemId,
                        MENU_ITEM_WITH_SUBTITLE.languageCode,
                        MENU_ITEM_WITH_SUBTITLE.hasCheckmark);

        onView(withText(MENU_ITEM_WITH_SUBTITLE.title)).check(doesNotExist());
    }

    @Test
    @MediumTest
    public void testOpenMenuAfterClearNativePointer() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    // Immediately clear the native object pointer from the TranslateMessage.
                    prepareListMenuButtonForTranslateMessageOnUiThread().clearNativePointer();
                });

        onView(withId(LIST_MENU_BUTTON_ID)).perform(click());

        // Since the native pointer was cleared, there should have been no calls to any native
        // methods.
        verifyNoMoreInteractions(mMockJni);
    }

    /**
     * Add a ListMenuButton that calls into the secondary menu of a newly created TranslateMessage.
     * Must be run on the UI thread.
     *
     * @return The newly created TranslateMessage.
     */
    private TranslateMessage prepareListMenuButtonForTranslateMessageOnUiThread() {
        TranslateMessage translateMessage =
                new TranslateMessage(
                        sActivity,
                        mMessageDispatcher,
                        mWebContents,
                        NATIVE_TRANSLATE_MESSAGE,
                        DISMISSAL_DURATION_SECONDS);

        translateMessage.showMessage(
                "Translate Page?", "French to English", "Translate", /* hasOverflowMenu= */ true);

        verify(mMessageDispatcher)
                .enqueueMessage(
                        mPropertyModelCaptor.capture(),
                        eq(mWebContents),
                        eq(MessageScopeType.NAVIGATION),
                        /* highPriority= */ eq(false));
        PropertyModel messageProperties = mPropertyModelCaptor.getValue();

        ListMenuButton listMenuButton = new ListMenuButton(sActivity, null);
        listMenuButton.setId(LIST_MENU_BUTTON_ID);
        listMenuButton.setImageResource(
                messageProperties.get(MessageBannerProperties.SECONDARY_ICON_RESOURCE_ID));
        listMenuButton.setDelegate(
                messageProperties.get(MessageBannerProperties.SECONDARY_MENU_BUTTON_DELEGATE));
        // For a view created in a test, we can make the view not important for accessibility
        // to prevent failures from AccessibilityChecks. Do not do this for views outside tests.
        listMenuButton.setImportantForAccessibility(View.IMPORTANT_FOR_ACCESSIBILITY_NO);

        sContentView.addView(listMenuButton);

        return translateMessage;
    }
}
