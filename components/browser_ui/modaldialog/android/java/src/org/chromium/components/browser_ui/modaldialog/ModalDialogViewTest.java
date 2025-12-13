// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.modaldialog;

import static android.view.ViewGroup.LayoutParams.MATCH_PARENT;
import static android.view.ViewGroup.LayoutParams.WRAP_CONTENT;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.assertion.ViewAssertions.doesNotExist;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.ViewMatchers.isChecked;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.isEnabled;
import static androidx.test.espresso.matcher.ViewMatchers.isNotChecked;
import static androidx.test.espresso.matcher.ViewMatchers.withChild;
import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withParent;
import static androidx.test.espresso.matcher.ViewMatchers.withTagValue;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.hamcrest.Matchers.allOf;
import static org.hamcrest.Matchers.is;
import static org.hamcrest.Matchers.not;
import static org.mockito.Mockito.times;

import android.app.Activity;
import android.content.res.ColorStateList;
import android.content.res.Resources;
import android.graphics.drawable.Drawable;
import android.os.Build;
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
import android.widget.ProgressBar;
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
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.FakeTimeTestRule;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.BaseActivityTestRule;
import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.DisableIf;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Feature;
import org.chromium.components.browser_ui.modaldialog.test.R;
import org.chromium.components.browser_ui.styles.SemanticColorUtils;
import org.chromium.components.browser_ui.widget.DualControlLayout;
import org.chromium.components.browser_ui.widget.ModalDialogViewUtils;
import org.chromium.components.browser_ui.widget.SpinnerButtonWrapper;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modaldialog.ModalDialogProperties.ModalDialogButtonSpec;
import org.chromium.ui.modaldialog.ModalDialogProperties.ModalDialogMenuItem;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.test.util.BlankUiTestActivity;

import java.util.ArrayList;

/** Tests for {@link ModalDialogView}. */
@RunWith(BaseJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
public class ModalDialogViewTest {
    @ClassRule
    public static BaseActivityTestRule<BlankUiTestActivity> activityTestRule =
            new BaseActivityTestRule<>(BlankUiTestActivity.class);

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();
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

    @Mock private ModalDialogProperties.Controller mMockController;

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
        onView(withId(R.id.message_paragraphs_container)).check(matches(not(isDisplayed())));
        onView(withId(R.id.custom_view_not_in_scrollable)).check(matches(not(isDisplayed())));
        onView(withId(R.id.button_bar)).check(matches(not(isDisplayed())));
        onView(withId(R.id.positive_button)).check(matches(allOf(not(isDisplayed()), isEnabled())));
        onView(withId(R.id.negative_button)).check(matches(allOf(not(isDisplayed()), isEnabled())));
        onView(withId(R.id.custom_button_bar))
                .check(matches(allOf(not(isDisplayed()), isEnabled())));
        onView(withId(R.id.modal_dialog_checkbox)).check(matches(not(isDisplayed())));
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
        onView(withId(R.id.message_paragraphs_container)).check(matches(not(isDisplayed())));

        // Set title to not scrollable and verify that non-scrollable title is displayed.
        ThreadUtils.runOnUiThreadBlocking(
                () -> model.set(ModalDialogProperties.TITLE_SCROLLABLE, false));
        onView(allOf(withId(R.id.title), withParent(withId(R.id.title_container))))
                .check(matches(allOf(isDisplayed(), withText(R.string.title))));
        onView(withId(R.id.title_container)).check(matches(isDisplayed()));
        onView(withId(R.id.scrollable_title_container)).check(matches(not(isDisplayed())));
        onView(withId(R.id.modal_dialog_title_scroll_view)).check(matches(not(isDisplayed())));
        onView(withId(R.id.message_paragraphs_container)).check(matches(not(isDisplayed())));
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
    public void testMessageParagraph1_Convenience() {
        // Verify that the message set via MESSAGE_PARAGRAPH_1 is displayed in the paragraphs
        // container.
        String msg = sResources.getString(R.string.more);
        PropertyModel model =
                createModel(mModelBuilder.with(ModalDialogProperties.MESSAGE_PARAGRAPH_1, msg));
        onView(withId(R.id.modal_dialog_title_scroll_view)).check(matches(isDisplayed()));
        onView(withId(R.id.message_paragraphs_container)).check(matches(isDisplayed()));

        // Check that the text is displayed and is the only paragraph.
        onView(withText(msg)).check(matches(isDisplayed()));
        Assert.assertEquals(
                "The container should have exactly one paragraph.",
                1,
                ((ViewGroup) mModalDialogView.getMessageParagraphAtIndexForTesting(0).getParent())
                        .getChildCount());
        Assert.assertEquals(
                "The message text is incorrect.",
                msg,
                mModalDialogView.getMessageParagraphAtIndexForTesting(0).getText().toString());

        // Set an empty message and verify that the paragraphs container is not shown.
        ThreadUtils.runOnUiThreadBlocking(
                () -> model.set(ModalDialogProperties.MESSAGE_PARAGRAPH_1, ""));
        onView(withId(R.id.modal_dialog_title_scroll_view)).check(matches(not(isDisplayed())));
        onView(withId(R.id.message_paragraphs_container)).check(matches(not(isDisplayed())));

        // Use CharSequence for the message.
        SpannableStringBuilder sb = new SpannableStringBuilder(msg);
        sb.setSpan(new ForegroundColorSpan(0xffff0000), 0, 1, Spannable.SPAN_EXCLUSIVE_EXCLUSIVE);
        ThreadUtils.runOnUiThreadBlocking(
                () -> model.set(ModalDialogProperties.MESSAGE_PARAGRAPH_1, sb));
        onView(withId(R.id.modal_dialog_title_scroll_view)).check(matches(isDisplayed()));
        onView(withId(R.id.message_paragraphs_container)).check(matches(isDisplayed()));

        // Check that the styled text is displayed correctly.
        onView(withText(msg)).check(matches(isDisplayed()));
        Assert.assertEquals(
                "The container should still have exactly one paragraph.",
                1,
                ((ViewGroup) mModalDialogView.getMessageParagraphAtIndexForTesting(0).getParent())
                        .getChildCount());
        Assert.assertEquals(
                "The CharSequence text is incorrect.",
                sb.toString(),
                mModalDialogView.getMessageParagraphAtIndexForTesting(0).getText().toString());
    }

    @Test
    @MediumTest
    @Feature({"ModalDialog"})
    public void testMessageParagraphs() {
        ArrayList<CharSequence> paragraphs = new ArrayList<>();
        String p1 = "This is the first paragraph.";
        String p2_original = "This is the original second paragraph.";
        paragraphs.add(p1);
        paragraphs.add(p2_original);

        ThreadUtils.runOnUiThreadBlocking(() -> mModalDialogView.setMessageParagraphs(paragraphs));

        // Replace the second paragraph in the ArrayList
        String p2_updated = "This is the updated second paragraph.";
        paragraphs.set(1, p2_updated);

        // Verify the views are correct.
        onView(withId(R.id.message_paragraphs_container)).check(matches(isDisplayed()));
        onView(withText(p1)).check(matches(isDisplayed()));
        onView(withText(p2_original)).check(matches(isDisplayed()));
        Assert.assertEquals(
                "Initial paragraph 0 has wrong text.",
                p1,
                mModalDialogView.getMessageParagraphAtIndexForTesting(0).getText().toString());
        Assert.assertEquals(
                "Initial paragraph 1 has wrong text.",
                p2_original,
                mModalDialogView.getMessageParagraphAtIndexForTesting(1).getText().toString());

        // Replace the 2nd paragraph in the view.
        ThreadUtils.runOnUiThreadBlocking(() -> mModalDialogView.setMessageParagraphs(paragraphs));

        // Verify only the 2nd paragraph changed.
        onView(withText(p2_original)).check(doesNotExist());

        onView(withId(R.id.message_paragraphs_container)).check(matches(isDisplayed()));
        onView(withText(p1)).check(matches(isDisplayed()));
        onView(withText(p2_updated)).check(matches(isDisplayed()));
        Assert.assertEquals(
                "Updated paragraph 0 has wrong text.",
                p1,
                mModalDialogView.getMessageParagraphAtIndexForTesting(0).getText().toString());
        Assert.assertEquals(
                "Updated paragraph 1 has wrong text.",
                p2_updated,
                mModalDialogView.getMessageParagraphAtIndexForTesting(1).getText().toString());
    }

    @Test
    @MediumTest
    @Feature({"ModalDialog"})
    public void testSetMessageParagraphs_clearsPreviousContent() {
        ArrayList<CharSequence> initialParagraphs = new ArrayList<>();
        String initialText = "This paragraph should be cleared.";
        initialParagraphs.add(initialText);

        // Verify that null call empties the view.
        ThreadUtils.runOnUiThreadBlocking(
                () -> mModalDialogView.setMessageParagraphs(initialParagraphs));
        onView(withText(initialText)).check(matches(isDisplayed()));
        onView(withId(R.id.message_paragraphs_container)).check(matches(isDisplayed()));
        ThreadUtils.runOnUiThreadBlocking(() -> mModalDialogView.setMessageParagraphs(null));
        onView(withText(initialText)).check(doesNotExist());
        onView(withId(R.id.message_paragraphs_container)).check(matches(not(isDisplayed())));

        // Verify that empty list call empties the view.
        ThreadUtils.runOnUiThreadBlocking(
                () -> mModalDialogView.setMessageParagraphs(initialParagraphs));
        onView(withText(initialText)).check(matches(isDisplayed()));
        onView(withId(R.id.message_paragraphs_container)).check(matches(isDisplayed()));
        ThreadUtils.runOnUiThreadBlocking(
                () -> mModalDialogView.setMessageParagraphs(new ArrayList<>()));
        onView(withText(initialText)).check(doesNotExist());
        onView(withId(R.id.message_paragraphs_container)).check(matches(not(isDisplayed())));
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
    public void testCheckbox_Visibility() {
        // Set checkbox to be visible by setting its text.
        PropertyModel model =
                createModel(
                        mModelBuilder.with(ModalDialogProperties.CHECKBOX_TEXT, "Make visible"));
        onView(withId(R.id.modal_dialog_checkbox)).check(matches(isDisplayed()));

        // Set checkbox to be not visible by clearing its text.
        ThreadUtils.runOnUiThreadBlocking(() -> model.set(ModalDialogProperties.CHECKBOX_TEXT, ""));
        onView(withId(R.id.modal_dialog_checkbox)).check(matches(not(isDisplayed())));
    }

    @Test
    @MediumTest
    @Feature({"ModalDialog"})
    public void testCheckbox_TextAndInitialState() {
        final String checkboxText = "Don't show this again";

        // Verify that the checkbox can be configured with text and an initial checked state.
        PropertyModel model =
                createModel(
                        mModelBuilder
                                .with(ModalDialogProperties.CHECKBOX_TEXT, checkboxText)
                                .with(ModalDialogProperties.CHECKBOX_CHECKED, true));

        onView(withId(R.id.modal_dialog_checkbox))
                .check(matches(allOf(isDisplayed(), withText(checkboxText), isChecked())));

        // Programmatically uncheck the checkbox and verify the view updates.
        ThreadUtils.runOnUiThreadBlocking(
                () -> model.set(ModalDialogProperties.CHECKBOX_CHECKED, false));
        onView(withId(R.id.modal_dialog_checkbox)).check(matches(isNotChecked()));
    }

    @Test
    @MediumTest
    @Feature({"ModalDialog"})
    public void testCheckbox_InteractionUpdatesModel() {
        final String checkboxText = "Opt-in for awesome features";

        PropertyModel model =
                createModel(
                        mModelBuilder
                                .with(ModalDialogProperties.CONTROLLER, mMockController)
                                .with(ModalDialogProperties.CHECKBOX_TEXT, checkboxText)
                                .with(ModalDialogProperties.CHECKBOX_CHECKED, false));

        // Verify initial state.
        onView(withId(R.id.modal_dialog_checkbox))
                .check(matches(allOf(isDisplayed(), isNotChecked())));
        Assert.assertFalse(
                "Model property CHECKBOX_CHECKED should be false initially.",
                model.get(ModalDialogProperties.CHECKBOX_CHECKED));

        // Perform a click to check the box.
        onView(withId(R.id.modal_dialog_checkbox)).perform(click());

        // Verify that the view is now checked AND the model property has been updated.
        onView(withId(R.id.modal_dialog_checkbox)).check(matches(isChecked()));
        Assert.assertTrue(
                "Model property CHECKBOX_CHECKED should be true after click.",
                model.get(ModalDialogProperties.CHECKBOX_CHECKED));
        Mockito.verify(mMockController, times(1)).onCheckboxChecked(true);

        // Perform another click to uncheck the box.
        onView(withId(R.id.modal_dialog_checkbox)).perform(click());

        // Verify that the view is now unchecked AND the model property has been updated.
        onView(withId(R.id.modal_dialog_checkbox)).check(matches(isNotChecked()));
        Assert.assertFalse(
                "Model property CHECKBOX_CHECKED should be false after second click.",
                model.get(ModalDialogProperties.CHECKBOX_CHECKED));
        Mockito.verify(mMockController, times(1)).onCheckboxChecked(false);
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
                        .with(ModalDialogProperties.POSITIVE_BUTTON_TEXT, sResources, R.string.ok)
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

    @Test
    @MediumTest
    @Feature({"ModalDialog"})
    public void testModalDialogCustomPositiveSpinnerButtonWidget() {
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

        PropertyModel model =
                createModel(
                        mModelBuilder
                                .with(
                                        ModalDialogProperties.BUTTON_STYLES,
                                        ModalDialogProperties.ButtonStyles
                                                .PRIMARY_FILLED_NEGATIVE_OUTLINE)
                                .with(
                                        ModalDialogProperties.POSITIVE_BUTTON_TEXT,
                                        sResources,
                                        R.string.ok)
                                .with(
                                        ModalDialogProperties.NEGATIVE_BUTTON_TEXT,
                                        sResources,
                                        R.string.cancel)
                                .with(ModalDialogProperties.CONTROLLER, controller));
        onView(withId(R.id.positive_button))
                .check(matches(allOf(isDisplayed(), isEnabled(), withText(R.string.ok))));
        onView(withId(R.id.negative_button))
                .check(matches(allOf(isDisplayed(), isEnabled(), withText(R.string.cancel))));

        Button primaryButton =
                DualControlLayout.createButtonForLayout(
                        sActivity,
                        DualControlLayout.ButtonType.PRIMARY_FILLED,
                        sResources.getString(R.string.ok),
                        null);
        Button secondaryButton =
                DualControlLayout.createButtonForLayout(
                        sActivity,
                        DualControlLayout.ButtonType.SECONDARY_TEXT,
                        sResources.getString(R.string.cancel),
                        null);
        SpinnerButtonWrapper spinnerButtonWrapperPositive =
                SpinnerButtonWrapper.createSpinnerButtonWrapper(
                        sActivity,
                        primaryButton,
                        R.string.ok,
                        R.dimen.modal_dialog_spinner_size,
                        SemanticColorUtils.getDefaultBgColor(sActivity),
                        () -> {
                            model.set(ModalDialogProperties.BLOCK_INPUTS, true);
                        });
        View customButtonBarView =
                ModalDialogViewUtils.createCustomButtonBarView(
                        sActivity, spinnerButtonWrapperPositive, secondaryButton);

        // Set up the custom button bar view with a positive button spinner and click
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    model.set(ModalDialogProperties.CUSTOM_BUTTON_BAR_VIEW, customButtonBarView);
                });
        Button positiveButton = spinnerButtonWrapperPositive.findViewById(R.id.button_primary);
        ProgressBar progressBar = spinnerButtonWrapperPositive.findViewById(R.id.progress_bar);
        Assert.assertEquals(View.GONE, progressBar.getVisibility());
        onView(withId(R.id.spinner_button)).perform(click());

        // Assert that the button properties are as expected
        Assert.assertEquals(View.VISIBLE, progressBar.getVisibility());
        Assert.assertEquals(0, positiveButton.getTextScaleX(), 0.0);
        Assert.assertEquals(
                ColorStateList.valueOf(SemanticColorUtils.getDefaultBgColor(sActivity)),
                progressBar.getIndeterminateTintList());

        // Assert that clicks on the modal dialog are disabled
        onView(withId(R.id.button_secondary)).perform(click());
        Assert.assertEquals(0, callbackHelper.getCallCount());
    }

    @Test
    @MediumTest
    @Feature({"ModalDialog"})
    public void testModalDialogCustomNegativeSpinnerButtonWidget() {
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

        PropertyModel model =
                createModel(
                        mModelBuilder
                                .with(
                                        ModalDialogProperties.BUTTON_STYLES,
                                        ModalDialogProperties.ButtonStyles
                                                .PRIMARY_FILLED_NEGATIVE_OUTLINE)
                                .with(
                                        ModalDialogProperties.POSITIVE_BUTTON_TEXT,
                                        sResources,
                                        R.string.ok)
                                .with(
                                        ModalDialogProperties.NEGATIVE_BUTTON_TEXT,
                                        sResources,
                                        R.string.cancel)
                                .with(ModalDialogProperties.CONTROLLER, controller));
        onView(withId(R.id.positive_button))
                .check(matches(allOf(isDisplayed(), isEnabled(), withText(R.string.ok))));
        onView(withId(R.id.negative_button))
                .check(matches(allOf(isDisplayed(), isEnabled(), withText(R.string.cancel))));

        Button primaryButton =
                DualControlLayout.createButtonForLayout(
                        sActivity,
                        DualControlLayout.ButtonType.PRIMARY_FILLED,
                        sResources.getString(R.string.ok),
                        null);
        Button secondaryButton =
                DualControlLayout.createButtonForLayout(
                        sActivity,
                        DualControlLayout.ButtonType.SECONDARY_TEXT,
                        sResources.getString(R.string.cancel),
                        null);
        SpinnerButtonWrapper spinnerButtonWrapperNegative =
                SpinnerButtonWrapper.createSpinnerButtonWrapper(
                        sActivity,
                        secondaryButton,
                        R.string.cancel,
                        R.dimen.modal_dialog_spinner_size,
                        SemanticColorUtils.getDefaultIconColorAccent1(sActivity),
                        () -> {
                            model.set(ModalDialogProperties.BLOCK_INPUTS, true);
                        });
        View customButtonBarView =
                ModalDialogViewUtils.createCustomButtonBarView(
                        sActivity, primaryButton, spinnerButtonWrapperNegative);

        // Set up the custom button bar view with a negative button spinner and click
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    model.set(ModalDialogProperties.CUSTOM_BUTTON_BAR_VIEW, customButtonBarView);
                });
        Button negativeButton = spinnerButtonWrapperNegative.findViewById(R.id.button_secondary);
        ProgressBar progressBar = spinnerButtonWrapperNegative.findViewById(R.id.progress_bar);
        Assert.assertEquals(View.GONE, progressBar.getVisibility());
        onView(withId(R.id.spinner_button)).perform(click());

        // Assert that the button properties are as expected
        Assert.assertEquals(View.VISIBLE, progressBar.getVisibility());
        Assert.assertEquals(0, negativeButton.getTextScaleX(), 0.0);
        Assert.assertEquals(
                ColorStateList.valueOf(SemanticColorUtils.getDefaultIconColorAccent1(sActivity)),
                progressBar.getIndeterminateTintList());

        // Assert that clicks on the modal dialog are disabled
        onView(withId(R.id.button_primary)).perform(click());
        Assert.assertEquals(0, callbackHelper.getCallCount());
    }

    @Test
    @MediumTest
    @Feature({"ModalDialog"})
    public void testMenuItem_Basic() {
        final String text1 = "Menu Item 1";
        final Drawable icon1 = sActivity.getDrawable(R.drawable.ic_business);
        ArrayList<ModalDialogMenuItem> menuItems = new ArrayList<>();
        menuItems.add(new ModalDialogMenuItem(icon1, text1));

        createModel(mModelBuilder.with(ModalDialogProperties.MENU_ITEMS, menuItems));

        onView(withId(R.id.menu_items_container)).check(matches(isDisplayed()));
        onView(withText(text1)).check(matches(isDisplayed()));

        LinearLayout menuItemsContainer = mModalDialogView.findViewById(R.id.menu_items_container);
        Assert.assertEquals(
                "Menu container should have one item.", 1, menuItemsContainer.getChildCount());
        TextView menuItemView = (TextView) menuItemsContainer.getChildAt(0);
        Assert.assertEquals(
                "Icon should match.",
                icon1.getConstantState(),
                menuItemView.getCompoundDrawablesRelative()[0].getConstantState());
    }

    @Test
    @MediumTest
    @Feature({"ModalDialog"})
    public void testMenuItems_Dynamic() {
        final String text1 = "First Menu Item";
        final Drawable icon1 = sActivity.getDrawable(R.drawable.ic_business);
        final String text2 = "Second Menu Item";
        final Drawable icon2 = sActivity.getDrawable(R.drawable.ic_business);
        ArrayList<ModalDialogMenuItem> menuItems = new ArrayList<>();
        menuItems.add(new ModalDialogMenuItem(icon1, text1));
        menuItems.add(new ModalDialogMenuItem(icon2, text2));

        PropertyModel model =
                createModel(mModelBuilder.with(ModalDialogProperties.MENU_ITEMS, menuItems));

        // Assert initial state with 2 items.
        onView(withId(R.id.menu_items_container)).check(matches(isDisplayed()));
        onView(withText(text1)).check(matches(isDisplayed()));
        onView(withText(text2)).check(matches(isDisplayed()));
        LinearLayout menuItemsContainer = mModalDialogView.findViewById(R.id.menu_items_container);
        Assert.assertEquals(
                "Menu container should have two items.", 2, menuItemsContainer.getChildCount());
        TextView menuItemView1 = (TextView) menuItemsContainer.getChildAt(0);
        Assert.assertEquals("Item 1 text mismatch.", text1, menuItemView1.getText().toString());
        Assert.assertEquals(
                "Item 1 icon mismatch.",
                icon1.getConstantState(),
                menuItemView1.getCompoundDrawablesRelative()[0].getConstantState());
        TextView menuItemView2 = (TextView) menuItemsContainer.getChildAt(1);
        Assert.assertEquals("Item 2 text mismatch.", text2, menuItemView2.getText().toString());
        Assert.assertEquals(
                "Item 2 icon mismatch.",
                icon2.getConstantState(),
                menuItemView2.getCompoundDrawablesRelative()[0].getConstantState());

        // Clear with null list.
        ThreadUtils.runOnUiThreadBlocking(() -> model.set(ModalDialogProperties.MENU_ITEMS, null));
        onView(withId(R.id.menu_items_container)).check(matches(not(isDisplayed())));
        onView(withText(text1)).check(doesNotExist());
        onView(withText(text2)).check(doesNotExist());
        Assert.assertEquals(
                "Menu container should be empty after setting null.",
                0,
                menuItemsContainer.getChildCount());

        // Re-add the same 2 items.
        ThreadUtils.runOnUiThreadBlocking(
                () -> model.set(ModalDialogProperties.MENU_ITEMS, menuItems));
        onView(withId(R.id.menu_items_container)).check(matches(isDisplayed()));
        onView(withText(text1)).check(matches(isDisplayed()));
        onView(withText(text2)).check(matches(isDisplayed()));
        Assert.assertEquals(
                "Menu container should have two items after re-adding.",
                2,
                menuItemsContainer.getChildCount());

        // Clear with empty list.
        ThreadUtils.runOnUiThreadBlocking(
                () -> model.set(ModalDialogProperties.MENU_ITEMS, new ArrayList<>()));
        onView(withId(R.id.menu_items_container)).check(matches(not(isDisplayed())));
        onView(withText(text1)).check(doesNotExist());
        onView(withText(text2)).check(doesNotExist());
        Assert.assertEquals(
                "Menu container should be empty after setting an empty list.",
                0,
                menuItemsContainer.getChildCount());
    }

    @Test
    @MediumTest
    @Feature({"ModalDialog"})
    @DisableIf.Build(
            sdk_is_greater_than = Build.VERSION_CODES.VANILLA_ICE_CREAM,
            message = "https://crbug.com/437920264")
    public void testMenuItem_Callback() throws Exception {
        final CallbackHelper callbackHelper = new CallbackHelper();
        final String text = "Menu Item with Callback";
        final Drawable icon = sActivity.getDrawable(R.drawable.ic_business);
        ArrayList<ModalDialogMenuItem> menuItems = new ArrayList<>();
        menuItems.add(new ModalDialogMenuItem(icon, text, callbackHelper::notifyCalled));

        createModel(mModelBuilder.with(ModalDialogProperties.MENU_ITEMS, menuItems));

        onView(withText(text)).perform(click());
        callbackHelper.waitForCallback(0);
    }

    private static Matcher<View> touchFilterEnabled() {
        return new TypeSafeMatcher<>() {
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
