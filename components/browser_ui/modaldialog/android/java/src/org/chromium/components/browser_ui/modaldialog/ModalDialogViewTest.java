// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.modaldialog;

import static android.view.ViewGroup.LayoutParams.MATCH_PARENT;
import static android.view.ViewGroup.LayoutParams.WRAP_CONTENT;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.isEnabled;
import static androidx.test.espresso.matcher.ViewMatchers.withChild;
import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withParent;
import static androidx.test.espresso.matcher.ViewMatchers.withTagValue;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.hamcrest.Matchers.allOf;
import static org.hamcrest.Matchers.is;
import static org.hamcrest.Matchers.not;

import android.app.Activity;
import android.content.res.Resources;
import android.text.Spannable;
import android.text.SpannableStringBuilder;
import android.text.style.ForegroundColorSpan;
import android.view.ContextThemeWrapper;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.Button;
import android.widget.FrameLayout;
import android.widget.LinearLayout;
import android.widget.RelativeLayout;
import android.widget.ScrollView;
import android.widget.TextView;

import androidx.test.espresso.action.ViewActions;
import androidx.test.filters.MediumTest;

import org.hamcrest.Description;
import org.hamcrest.Matcher;
import org.hamcrest.TypeSafeMatcher;
import org.junit.Assert;
import org.junit.Before;
import org.junit.BeforeClass;
import org.junit.ClassRule;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.FakeTimeTestRule;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.BaseActivityTestRule;
import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Feature;
import org.chromium.components.browser_ui.modaldialog.test.R;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modaldialog.ModalDialogProperties.ModalDialogButtonSpec;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.test.util.BlankUiTestActivity;

/** Tests for {@link ModalDialogView}. */
@RunWith(BaseJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
public class ModalDialogViewTest {
    @ClassRule
    public static BaseActivityTestRule<BlankUiTestActivity> activityTestRule =
            new BaseActivityTestRule<>(BlankUiTestActivity.class);

    @Rule public FakeTimeTestRule mFakeTime = new FakeTimeTestRule();

    private static Activity sActivity;
    private static Resources sResources;
    private static FrameLayout sContentView;
    private ModalDialogView mModalDialogView;
    private TextView mCustomTextView1;
    private TextView mCustomTextView2;
    private PropertyModel.Builder mModelBuilder;
    private RelativeLayout mCustomButtonBar1;
    private RelativeLayout mCustomButtonBar2;

    @BeforeClass
    public static void setupSuite() {
        activityTestRule.launchActivity(null);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    sActivity = activityTestRule.getActivity();
                    sResources = sActivity.getResources();
                    sContentView = new FrameLayout(sActivity);
                    sActivity.setContentView(sContentView);
                });
    }

    @Before
    public void setupTest() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    sContentView.removeAllViews();
                    mModelBuilder = new PropertyModel.Builder(ModalDialogProperties.ALL_KEYS);
                    mModalDialogView =
                            (ModalDialogView)
                                    LayoutInflater.from(
                                                    new ContextThemeWrapper(
                                                            sActivity,
                                                            R.style
                                                                    .ThemeOverlay_BrowserUI_ModalDialog_TextPrimaryButton))
                                            .inflate(R.layout.modal_dialog_view, null);
                    sContentView.addView(mModalDialogView, MATCH_PARENT, WRAP_CONTENT);

                    mCustomTextView1 = new TextView(sActivity);
                    mCustomTextView1.setId(R.id.test_view_one);
                    mCustomTextView2 = new TextView(sActivity);
                    mCustomTextView2.setId(R.id.test_view_two);

                    mCustomButtonBar1 = new RelativeLayout(sActivity);
                    mCustomButtonBar1.setId(R.id.test_button_bar_one);
                    mCustomButtonBar2 = new RelativeLayout(sActivity);
                    mCustomButtonBar2.setId(R.id.test_button_bar_two);
                    Button button1 = new Button(sActivity);
                    button1.setText(R.string.ok);
                    Button button2 = new Button(sActivity);
                    button2.setText(R.string.cancel);
                    RelativeLayout.LayoutParams params =
                            new RelativeLayout.LayoutParams(
                                    ViewGroup.LayoutParams.WRAP_CONTENT,
                                    ViewGroup.LayoutParams.WRAP_CONTENT);
                    params.addRule(RelativeLayout.ALIGN_PARENT_LEFT, RelativeLayout.TRUE);
                    mCustomButtonBar1.addView(button1, params);
                    mCustomButtonBar2.addView(button2, params);
                });
    }

    @Test
    @MediumTest
    @Feature({"ModalDialog"})
    public void testInitialStates() {
        // Verify that the default states are correct when properties are not set.
        createModel(mModelBuilder);
        onView(withId(R.id.title_container)).check(matches(not(isDisplayed())));
        onView(withId(R.id.scrollable_title_container)).check(matches(not(isDisplayed())));
        onView(withId(R.id.modal_dialog_title_scroll_view)).check(matches(not(isDisplayed())));
        onView(withId(R.id.message_paragraph_1)).check(matches(not(isDisplayed())));
        onView(withId(R.id.message_paragraph_2)).check(matches(not(isDisplayed())));
        onView(withId(R.id.custom_view_not_in_scrollable)).check(matches(not(isDisplayed())));
        onView(withId(R.id.button_bar)).check(matches(not(isDisplayed())));
        onView(withId(R.id.positive_button)).check(matches(allOf(not(isDisplayed()), isEnabled())));
        onView(withId(R.id.negative_button)).check(matches(allOf(not(isDisplayed()), isEnabled())));
        onView(withId(R.id.custom_button_bar))
                .check(matches(allOf(not(isDisplayed()), isEnabled())));
    }

    @Test
    @MediumTest
    @Feature({"ModalDialog"})
    public void testTitle() {
        // Verify that the title set from builder is displayed.
        PropertyModel model =
                createModel(
                        mModelBuilder.with(
                                ModalDialogProperties.TITLE, sResources, R.string.title));
        onView(allOf(withId(R.id.title), withParent(withId(R.id.title_container))))
                .check(matches(allOf(isDisplayed(), withText(R.string.title))));
        onView(withId(R.id.title_container)).check(matches(isDisplayed()));
        onView(withId(R.id.modal_dialog_title_scroll_view)).check(matches(not(isDisplayed())));

        // Set an empty title and verify that title is not shown.
        ThreadUtils.runOnUiThreadBlocking(() -> model.set(ModalDialogProperties.TITLE, ""));
        onView(allOf(withId(R.id.title), withParent(withId(R.id.title_container))))
                .check(matches(not(isDisplayed())));
        onView(withId(R.id.title_container)).check(matches(not(isDisplayed())));
        onView(withId(R.id.modal_dialog_title_scroll_view)).check(matches(not(isDisplayed())));

        // Set a String title and verify that title is displayed.
        ThreadUtils.runOnUiThreadBlocking(
                () -> model.set(ModalDialogProperties.TITLE, "My Test Title"));
        onView(allOf(withId(R.id.title), withParent(withId(R.id.title_container))))
                .check(matches(allOf(isDisplayed(), withText("My Test Title"))));
        onView(withId(R.id.title_container)).check(matches(isDisplayed()));
        onView(withId(R.id.modal_dialog_title_scroll_view)).check(matches(not(isDisplayed())));
    }

    @Test
    @MediumTest
    @Feature({"ModalDialog"})
    public void testTitle_Scrollable() {
        // Verify that the title set from builder is displayed.
        PropertyModel model =
                createModel(
                        mModelBuilder
                                .with(ModalDialogProperties.TITLE, sResources, R.string.title)
                                .with(ModalDialogProperties.TITLE_SCROLLABLE, true));
        onView(allOf(withId(R.id.title), withParent(withId(R.id.scrollable_title_container))))
                .check(matches(allOf(isDisplayed(), withText(R.string.title))));
        onView(withId(R.id.title_container)).check(matches(not(isDisplayed())));
        onView(withId(R.id.scrollable_title_container)).check(matches(isDisplayed()));
        onView(withId(R.id.modal_dialog_title_scroll_view)).check(matches(isDisplayed()));
        onView(withId(R.id.message_paragraph_1)).check(matches(not(isDisplayed())));

        // Set title to not scrollable and verify that non-scrollable title is displayed.
        ThreadUtils.runOnUiThreadBlocking(
                () -> model.set(ModalDialogProperties.TITLE_SCROLLABLE, false));
        onView(allOf(withId(R.id.title), withParent(withId(R.id.title_container))))
                .check(matches(allOf(isDisplayed(), withText(R.string.title))));
        onView(withId(R.id.title_container)).check(matches(isDisplayed()));
        onView(withId(R.id.scrollable_title_container)).check(matches(not(isDisplayed())));
        onView(withId(R.id.modal_dialog_title_scroll_view)).check(matches(not(isDisplayed())));
        onView(withId(R.id.message_paragraph_1)).check(matches(not(isDisplayed())));
    }

    @Test
    @MediumTest
    @Feature({"ModalDialog"})
    public void testTitleIcon() {
        // Verify that the icon set from builder is displayed.
        PropertyModel model =
                createModel(
                        mModelBuilder.with(
                                ModalDialogProperties.TITLE_ICON,
                                sActivity,
                                R.drawable.ic_business));
        onView(allOf(withId(R.id.title), withParent(withId(R.id.title_container))))
                .check(matches(not(isDisplayed())));
        onView(allOf(withId(R.id.title_icon), withParent(withId(R.id.title_container))))
                .check(matches(isDisplayed()));
        onView(withId(R.id.title_container)).check(matches(isDisplayed()));
        onView(withId(R.id.scrollable_title_container)).check(matches(not(isDisplayed())));

        // Set icon to null and verify that icon is not shown.
        ThreadUtils.runOnUiThreadBlocking(() -> model.set(ModalDialogProperties.TITLE_ICON, null));
        onView(allOf(withId(R.id.title), withParent(withId(R.id.title_container))))
                .check(matches(not(isDisplayed())));
        onView(allOf(withId(R.id.title_icon), withParent(withId(R.id.title_container))))
                .check(matches(not(isDisplayed())));
        onView(withId(R.id.title_container)).check(matches(not(isDisplayed())));
        onView(withId(R.id.scrollable_title_container)).check(matches(not(isDisplayed())));
    }

    @Test
    @MediumTest
    @Feature({"ModalDialog"})
    public void testMessageParagraph1() {
        // Verify that the message_paragraph_1 set from builder is displayed.
        String msg = sResources.getString(R.string.more);
        PropertyModel model =
                createModel(mModelBuilder.with(ModalDialogProperties.MESSAGE_PARAGRAPH_1, msg));
        onView(withId(R.id.title_container)).check(matches(not(isDisplayed())));
        onView(withId(R.id.scrollable_title_container)).check(matches(not(isDisplayed())));
        onView(withId(R.id.modal_dialog_title_scroll_view)).check(matches(isDisplayed()));
        onView(withId(R.id.message_paragraph_1))
                .check(matches(allOf(isDisplayed(), withText(R.string.more))));

        // Set an empty message_paragraph_1 and verify that message_paragraph_1 is not shown.
        ThreadUtils.runOnUiThreadBlocking(
                () -> model.set(ModalDialogProperties.MESSAGE_PARAGRAPH_1, ""));
        onView(withId(R.id.title_container)).check(matches(not(isDisplayed())));
        onView(withId(R.id.scrollable_title_container)).check(matches(not(isDisplayed())));
        onView(withId(R.id.modal_dialog_title_scroll_view)).check(matches(not(isDisplayed())));
        onView(withId(R.id.message_paragraph_1)).check(matches(not(isDisplayed())));

        // Use CharSequence for the message_paragraph_1.
        SpannableStringBuilder sb = new SpannableStringBuilder(msg);
        sb.setSpan(new ForegroundColorSpan(0xffff0000), 0, 1, Spannable.SPAN_EXCLUSIVE_EXCLUSIVE);
        ThreadUtils.runOnUiThreadBlocking(
                () -> model.set(ModalDialogProperties.MESSAGE_PARAGRAPH_1, sb));
        onView(withId(R.id.title_container)).check(matches(not(isDisplayed())));
        onView(withId(R.id.scrollable_title_container)).check(matches(not(isDisplayed())));
        onView(withId(R.id.modal_dialog_title_scroll_view)).check(matches(isDisplayed()));
        onView(withId(R.id.message_paragraph_1))
                .check(matches(allOf(isDisplayed(), withText(R.string.more))));
    }

    @Test
    @MediumTest
    @Feature({"ModalDialog"})
    public void testMessageParagraph2() {
        // Verify that the message_paragraph_2 set from builder is displayed.
        String msg = "Incognito warning message";
        PropertyModel model =
                createModel(mModelBuilder.with(ModalDialogProperties.MESSAGE_PARAGRAPH_2, msg));
        onView(withId(R.id.title_container)).check(matches(not(isDisplayed())));
        onView(withId(R.id.scrollable_title_container)).check(matches(not(isDisplayed())));
        onView(withId(R.id.modal_dialog_title_scroll_view)).check(matches(isDisplayed()));
        onView(withId(R.id.message_paragraph_1)).check(matches(not(isDisplayed())));
        onView(withId(R.id.message_paragraph_2))
                .check(matches(allOf(isDisplayed(), withText(msg))));

        // Set an empty message_paragraph_2 and verify that it's not shown.
        ThreadUtils.runOnUiThreadBlocking(
                () -> model.set(ModalDialogProperties.MESSAGE_PARAGRAPH_2, ""));
        onView(withId(R.id.title_container)).check(matches(not(isDisplayed())));
        onView(withId(R.id.scrollable_title_container)).check(matches(not(isDisplayed())));
        onView(withId(R.id.modal_dialog_title_scroll_view)).check(matches(not(isDisplayed())));
        onView(withId(R.id.message_paragraph_2)).check(matches(not(isDisplayed())));
    }

    @Test
    @MediumTest
    @Feature({"ModalDialog"})
    public void testCustomView() {
        // Verify custom view set from builder is displayed.
        PropertyModel model =
                createModel(
                        mModelBuilder.with(ModalDialogProperties.CUSTOM_VIEW, mCustomTextView1));
        onView(withId(R.id.custom_view_not_in_scrollable))
                .check(matches(allOf(isDisplayed(), withChild(withId(R.id.test_view_one)))));

        // Change custom view.
        ThreadUtils.runOnUiThreadBlocking(
                () -> model.set(ModalDialogProperties.CUSTOM_VIEW, mCustomTextView2));
        onView(withId(R.id.custom_view_not_in_scrollable))
                .check(
                        matches(
                                allOf(
                                        isDisplayed(),
                                        not(withChild(withId(R.id.test_view_one))),
                                        withChild(withId(R.id.test_view_two)))));

        // Set custom view to null.
        ThreadUtils.runOnUiThreadBlocking(() -> model.set(ModalDialogProperties.CUSTOM_VIEW, null));
        onView(withId(R.id.custom_view_not_in_scrollable))
                .check(
                        matches(
                                allOf(
                                        not(isDisplayed()),
                                        not(withChild(withId(R.id.test_view_one))),
                                        not(withChild(withId(R.id.test_view_two))))));
    }

    @Test
    @MediumTest
    @Feature({"ModalDialog"})
    public void testScrollCustomView() {
        // Verify custom view set from builder is displayed.
        var scrollView = new ScrollView(activityTestRule.getActivity());
        var linearLayout = new LinearLayout(activityTestRule.getActivity());
        linearLayout.setOrientation(LinearLayout.VERTICAL);
        createModel(mModelBuilder.with(ModalDialogProperties.CUSTOM_VIEW, scrollView));
        // Add content.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    for (int i = 0; i < 100; i++) {
                        var textView = new TextView(activityTestRule.getActivity());
                        textView.setText(String.valueOf(i));
                        linearLayout.addView(textView);
                    }
                    scrollView.addView(linearLayout);
                    scrollView.setFillViewport(true);
                });
        // Verify the first few elements are visible.
        onView(withText("1")).check(matches(isDisplayed()));
        scrollView.scrollTo(0, scrollView.getBottom());
        // Verify after scrolling, the few elements are not visible.
        onView(withText("1")).check(matches(not(isDisplayed())));
    }

    @Test
    @MediumTest
    @Feature({"ModalDialog"})
    public void testCustomButtonBarView() {
        // Verify custom button bar view set from builder is displayed.
        PropertyModel model =
                createModel(
                        mModelBuilder
                                .with(
                                        ModalDialogProperties.CUSTOM_BUTTON_BAR_VIEW,
                                        mCustomButtonBar1)
                                .with(
                                        ModalDialogProperties.POSITIVE_BUTTON_TEXT,
                                        sResources,
                                        R.string.ok)
                                .with(
                                        ModalDialogProperties.NEGATIVE_BUTTON_TEXT,
                                        sResources,
                                        R.string.cancel));
        onView(withId(R.id.custom_button_bar))
                .check(matches(allOf(isDisplayed(), withChild(withId(R.id.test_button_bar_one)))));

        // There are no positive and negative buttons when the custom button bar is present.
        onView(withId(R.id.button_bar)).check(matches(not(isDisplayed())));
        onView(withId(R.id.positive_button)).check(matches(not(isDisplayed())));
        onView(withId(R.id.negative_button)).check(matches(not(isDisplayed())));

        // Change custom button bar view.
        ThreadUtils.runOnUiThreadBlocking(
                () -> model.set(ModalDialogProperties.CUSTOM_BUTTON_BAR_VIEW, mCustomButtonBar2));
        onView(withId(R.id.custom_button_bar))
                .check(matches(allOf(isDisplayed(), withChild(withId(R.id.test_button_bar_two)))));

        // Set custom button bar view to null.
        ThreadUtils.runOnUiThreadBlocking(
                () -> model.set(ModalDialogProperties.CUSTOM_BUTTON_BAR_VIEW, null));
        onView(withId(R.id.custom_button_bar)).check(matches(not(isDisplayed())));

        // The positive and negative buttons are back since the custom button bar is not there.
        onView(withId(R.id.button_bar)).check(matches(isDisplayed()));
        onView(withId(R.id.positive_button)).check(matches(isDisplayed()));
        onView(withId(R.id.negative_button)).check(matches(isDisplayed()));
    }

    @Test
    @MediumTest
    @Feature({"ModalDialog"})
    public void testButtonBar() {
        // Set text for both positive button and negative button.
        PropertyModel model =
                createModel(
                        mModelBuilder
                                .with(
                                        ModalDialogProperties.POSITIVE_BUTTON_TEXT,
                                        sResources,
                                        R.string.ok)
                                .with(
                                        ModalDialogProperties.NEGATIVE_BUTTON_TEXT,
                                        sResources,
                                        R.string.cancel));
        onView(withId(R.id.button_bar)).check(matches(isDisplayed()));
        onView(withId(R.id.positive_button))
                .check(matches(allOf(isDisplayed(), isEnabled(), withText(R.string.ok))));
        onView(withId(R.id.negative_button))
                .check(matches(allOf(isDisplayed(), isEnabled(), withText(R.string.cancel))));

        // Set positive button to be disabled state.
        ThreadUtils.runOnUiThreadBlocking(
                () -> model.set(ModalDialogProperties.POSITIVE_BUTTON_DISABLED, true));
        onView(withId(R.id.button_bar)).check(matches(isDisplayed()));
        onView(withId(R.id.positive_button))
                .check(matches(allOf(isDisplayed(), not(isEnabled()), withText(R.string.ok))));
        onView(withId(R.id.negative_button))
                .check(matches(allOf(isDisplayed(), isEnabled(), withText(R.string.cancel))));

        // Set positive button text to empty.
        ThreadUtils.runOnUiThreadBlocking(
                () -> model.set(ModalDialogProperties.POSITIVE_BUTTON_TEXT, ""));
        onView(withId(R.id.button_bar)).check(matches(isDisplayed()));
        onView(withId(R.id.positive_button)).check(matches(not(isDisplayed())));
        onView(withId(R.id.negative_button))
                .check(matches(allOf(isDisplayed(), isEnabled(), withText(R.string.cancel))));

        // Set negative button to be disabled state.
        ThreadUtils.runOnUiThreadBlocking(
                () -> model.set(ModalDialogProperties.NEGATIVE_BUTTON_DISABLED, true));
        onView(withId(R.id.button_bar)).check(matches(isDisplayed()));
        onView(withId(R.id.positive_button)).check(matches(not(isDisplayed())));
        onView(withId(R.id.negative_button))
                .check(matches(allOf(isDisplayed(), not(isEnabled()), withText(R.string.cancel))));

        // Set negative button text to empty.
        ThreadUtils.runOnUiThreadBlocking(
                () -> model.set(ModalDialogProperties.NEGATIVE_BUTTON_TEXT, ""));
        onView(withId(R.id.button_bar)).check(matches(not(isDisplayed())));
        onView(withId(R.id.positive_button)).check(matches(not(isDisplayed())));
        onView(withId(R.id.negative_button)).check(matches(not(isDisplayed())));
    }

    @Test
    @MediumTest
    @Feature({"ModalDialog"})
    public void testButtonGroup() {
        createModel(
                mModelBuilder.with(
                        ModalDialogProperties.BUTTON_GROUP_BUTTON_SPEC_LIST,
                        new ModalDialogProperties.ModalDialogButtonSpec[] {
                            new ModalDialogProperties.ModalDialogButtonSpec(
                                    ModalDialogProperties.ButtonType.POSITIVE_EPHEMERAL,
                                    sResources.getString(R.string.ok)),
                            new ModalDialogProperties.ModalDialogButtonSpec(
                                    ModalDialogProperties.ButtonType.POSITIVE,
                                    sResources.getString(R.string.ok_got_it)),
                            new ModalDialogProperties.ModalDialogButtonSpec(
                                    ModalDialogProperties.ButtonType.NEGATIVE,
                                    sResources.getString(R.string.cancel))
                        }));

        onView(withId(R.id.button_group)).check(matches(isDisplayed()));

        onView(withText(R.string.ok)).check(matches(isDisplayed()));
        onView(withText(R.string.ok_got_it)).check(matches(isDisplayed()));
        onView(withText(R.string.cancel)).check(matches(isDisplayed()));
    }

    @Test
    @MediumTest
    @Feature({"ModalDialog"})
    @DisabledTest(message = "crbug.com/329163841")
    public void testButtonGroupIsScrollable() throws InterruptedException {
        ModalDialogProperties.ModalDialogButtonSpec[] button_spec_list =
                new ModalDialogButtonSpec[20];
        for (int i = 0; i < button_spec_list.length; i++) {
            button_spec_list[i] =
                    new ModalDialogProperties.ModalDialogButtonSpec(
                            1000 + i, // ModalDialogProperties.ButtonType defines a button enum.
                            // Choose values outside the defined range.
                            sResources.getString(R.string.ok));
        }

        createModel(
                mModelBuilder.with(
                        ModalDialogProperties.BUTTON_GROUP_BUTTON_SPEC_LIST, button_spec_list));

        // Check that the first button is visible.
        onView(
                        withTagValue(
                                is(
                                        ModalDialogView.getTagForButtonType(
                                                button_spec_list[0].getButtonType()))))
                .check(matches(isDisplayed()));

        // Swipe up a few times.
        for (int i = 0; i < 3; i++) {
            onView(
                            withTagValue(
                                    is(
                                            ModalDialogView.getTagForButtonType(
                                                    button_spec_list[3].getButtonType()))))
                    .perform(ViewActions.swipeUp());
        }

        // Check that the first button is no longer visible.
        onView(
                        withTagValue(
                                is(
                                        ModalDialogView.getTagForButtonType(
                                                button_spec_list[0].getButtonType()))))
                .check(matches(not(isDisplayed())));
    }

    @Test
    @MediumTest
    @Feature({"ModalDialog"})
    public void testTouchFilter() {
        createModel(
                mModelBuilder
                        .with(ModalDialogProperties.POSITIVE_BUTTON_TEXT, sResources, R.string.ok)
                        .with(
                                ModalDialogProperties.NEGATIVE_BUTTON_TEXT,
                                sResources,
                                R.string.cancel)
                        .with(ModalDialogProperties.FILTER_TOUCH_FOR_SECURITY, true));
        onView(withId(R.id.positive_button)).check(matches(touchFilterEnabled()));
        onView(withId(R.id.negative_button)).check(matches(touchFilterEnabled()));
    }

    @Test
    @MediumTest
    @Feature({"ModalDialog"})
    public void testTouchFilterOnButtonGroup() {
        createModel(
                mModelBuilder
                        .with(
                                ModalDialogProperties.BUTTON_GROUP_BUTTON_SPEC_LIST,
                                new ModalDialogProperties.ModalDialogButtonSpec[] {
                                    new ModalDialogProperties.ModalDialogButtonSpec(
                                            ModalDialogProperties.ButtonType.POSITIVE_EPHEMERAL,
                                            sResources.getString(R.string.ok)),
                                    new ModalDialogProperties.ModalDialogButtonSpec(
                                            ModalDialogProperties.ButtonType.POSITIVE,
                                            sResources.getString(R.string.ok_got_it)),
                                    new ModalDialogProperties.ModalDialogButtonSpec(
                                            ModalDialogProperties.ButtonType.NEGATIVE,
                                            sResources.getString(R.string.cancel))
                                })
                        .with(ModalDialogProperties.FILTER_TOUCH_FOR_SECURITY, true));
        onView(
                        allOf(
                                withTagValue(
                                        is(
                                                ModalDialogView.getTagForButtonType(
                                                        ModalDialogProperties.ButtonType
                                                                .POSITIVE_EPHEMERAL))),
                                isDisplayed()))
                .check(matches(touchFilterEnabled()));
        onView(
                        allOf(
                                withTagValue(
                                        is(
                                                ModalDialogView.getTagForButtonType(
                                                        ModalDialogProperties.ButtonType
                                                                .POSITIVE))),
                                isDisplayed()))
                .check(matches(touchFilterEnabled()));
        onView(
                        allOf(
                                withTagValue(
                                        is(
                                                ModalDialogView.getTagForButtonType(
                                                        ModalDialogProperties.ButtonType
                                                                .NEGATIVE))),
                                isDisplayed()))
                .check(matches(touchFilterEnabled()));
    }

    @Test
    @MediumTest
    @Feature({"ModalDialog"})
    public void testTouchFilterDisabled() {
                createModel(
                        mModelBuilder
                                .with(
                                        ModalDialogProperties.POSITIVE_BUTTON_TEXT,
                                        sResources,
                                        R.string.ok)
                                .with(
                                        ModalDialogProperties.NEGATIVE_BUTTON_TEXT,
                                        sResources,
                                        R.string.cancel));
        onView(withId(R.id.positive_button)).check(matches(not(touchFilterEnabled())));
        onView(withId(R.id.negative_button)).check(matches(not(touchFilterEnabled())));
    }

    @Test
    @MediumTest
    @Feature({"ModalDialog"})
    public void testTouchFilterDisabledOnButtonGroup() {
        createModel(
                mModelBuilder.with(
                        ModalDialogProperties.BUTTON_GROUP_BUTTON_SPEC_LIST,
                        new ModalDialogProperties.ModalDialogButtonSpec[] {
                            new ModalDialogProperties.ModalDialogButtonSpec(
                                    ModalDialogProperties.ButtonType.POSITIVE_EPHEMERAL,
                                    sResources.getString(R.string.ok)),
                            new ModalDialogProperties.ModalDialogButtonSpec(
                                    ModalDialogProperties.ButtonType.POSITIVE,
                                    sResources.getString(R.string.ok_got_it)),
                            new ModalDialogProperties.ModalDialogButtonSpec(
                                    ModalDialogProperties.ButtonType.NEGATIVE,
                                    sResources.getString(R.string.cancel))
                        }));
        onView(
                        allOf(
                                withTagValue(
                                        is(
                                                ModalDialogView.getTagForButtonType(
                                                        ModalDialogProperties.ButtonType
                                                                .POSITIVE_EPHEMERAL))),
                                isDisplayed()))
                .check(matches(not(touchFilterEnabled())));
        onView(
                        allOf(
                                withTagValue(
                                        is(
                                                ModalDialogView.getTagForButtonType(
                                                        ModalDialogProperties.ButtonType
                                                                .POSITIVE))),
                                isDisplayed()))
                .check(matches(not(touchFilterEnabled())));
        onView(
                        allOf(
                                withTagValue(
                                        is(
                                                ModalDialogView.getTagForButtonType(
                                                        ModalDialogProperties.ButtonType
                                                                .NEGATIVE))),
                                isDisplayed()))
                .check(matches(not(touchFilterEnabled())));
    }

    @Test
    @MediumTest
    @Feature({"ModalDialog"})
    public void testFooterMessage() {
        // Verify that the footer message set from builder is displayed.
        String msg = sResources.getString(R.string.more);
        PropertyModel model =
                createModel(mModelBuilder.with(ModalDialogProperties.FOOTER_MESSAGE, msg));
        onView(withId(R.id.footer)).check(matches(isDisplayed()));
        onView(withId(R.id.footer_message))
                .check(matches(allOf(isDisplayed(), withText(R.string.more))));

        // Set an empty footer message and verify that footer message is not shown.
        ThreadUtils.runOnUiThreadBlocking(
                () -> model.set(ModalDialogProperties.FOOTER_MESSAGE, ""));
        onView(withId(R.id.footer)).check(matches(not(isDisplayed())));
        onView(withId(R.id.footer_message)).check(matches(not(isDisplayed())));

        // Use CharSequence for the footer message.
        SpannableStringBuilder sb = new SpannableStringBuilder(msg);
        sb.setSpan(new ForegroundColorSpan(0xffff0000), 0, 1, Spannable.SPAN_EXCLUSIVE_EXCLUSIVE);
        ThreadUtils.runOnUiThreadBlocking(
                () -> model.set(ModalDialogProperties.FOOTER_MESSAGE, sb));
        onView(withId(R.id.footer)).check(matches(isDisplayed()));
        onView(withId(R.id.footer_message))
                .check(matches(allOf(isDisplayed(), withText(R.string.more))));
    }

    @Test
    @MediumTest
    @Feature({"ModalDialog"})
    public void testButtonTapProtection() {
        final var callbackHelper = new CallbackHelper();
        var controller =
                new ModalDialogProperties.Controller() {
                    @Override
                    public void onClick(PropertyModel model, int buttonType) {
                        callbackHelper.notifyCalled();
                    }

                    @Override
                    public void onDismiss(PropertyModel model, int dismissalCause) {}
                };
        createModel(
                mModelBuilder
                        .with(ModalDialogProperties.POSITIVE_BUTTON_TEXT, sResources, R.string.ok)
                        .with(
                                ModalDialogProperties.NEGATIVE_BUTTON_TEXT,
                                sResources,
                                R.string.cancel)
                        .with(ModalDialogProperties.BUTTON_TAP_PROTECTION_PERIOD_MS, 100)
                        .with(ModalDialogProperties.CONTROLLER, controller));
        onView(withId(R.id.button_bar)).check(matches(isDisplayed()));
        mModalDialogView.onEnterAnimationStarted(0);
        onView(withId(R.id.positive_button)).perform(click());
        Assert.assertEquals(
                "Not accept click event when button is frozen.", 0, callbackHelper.getCallCount());
        mFakeTime.advanceMillis(200);
        onView(withId(R.id.positive_button)).perform(click());
        Assert.assertEquals(
                "Button is clickable after time elapses", 1, callbackHelper.getCallCount());
    }

    @Test
    @MediumTest
    @Feature({"ModalDialog"})
    public void testButtonTapProtectionForButtonGroup() {
        final var callbackHelper = new CallbackHelper();
        var controller =
                new ModalDialogProperties.Controller() {
                    @Override
                    public void onClick(PropertyModel model, int buttonType) {
                        callbackHelper.notifyCalled();
                    }

                    @Override
                    public void onDismiss(PropertyModel model, int dismissalCause) {}
                };

        createModel(
                mModelBuilder
                        .with(
                                ModalDialogProperties.BUTTON_GROUP_BUTTON_SPEC_LIST,
                                new ModalDialogProperties.ModalDialogButtonSpec[] {
                                    new ModalDialogProperties.ModalDialogButtonSpec(
                                            ModalDialogProperties.ButtonType.POSITIVE_EPHEMERAL,
                                            sResources.getString(R.string.ok))
                                })
                        .with(ModalDialogProperties.BUTTON_TAP_PROTECTION_PERIOD_MS, 100)
                        .with(ModalDialogProperties.CONTROLLER, controller));
        onView(withId(R.id.button_group)).check(matches(isDisplayed()));

        mModalDialogView.onEnterAnimationStarted(0);
        onView(withText(R.string.ok)).perform(click());
        Assert.assertEquals(
                "Not accept click event when button is frozen.", 0, callbackHelper.getCallCount());
        mFakeTime.advanceMillis(200);
        onView(withText(R.string.ok)).perform(click());
        Assert.assertEquals(
                "Button is clickable after time elapses", 1, callbackHelper.getCallCount());
    }

    private static Matcher<View> touchFilterEnabled() {
        return new TypeSafeMatcher<View>() {
            @Override
            public void describeTo(Description description) {
                description.appendText("Touch filtering enabled");
            }

            @Override
            public boolean matchesSafely(View view) {
                return view.getFilterTouchesWhenObscured();
            }
        };
    }

    private PropertyModel createModel(PropertyModel.Builder modelBuilder) {
        return ModalDialogTestUtils.createModel(modelBuilder, mModalDialogView);
    }
}
