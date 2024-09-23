// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.messages;

import static org.chromium.base.test.util.Restriction.RESTRICTION_TYPE_LOW_END_DEVICE;
import static org.chromium.base.test.util.Restriction.RESTRICTION_TYPE_NON_LOW_END_DEVICE;

import android.app.Activity;
import android.graphics.Bitmap;
import android.graphics.Color;
import android.graphics.Typeface;
import android.graphics.drawable.BitmapDrawable;
import android.graphics.drawable.Drawable;
import android.text.SpannableString;
import android.text.Spanned;
import android.text.style.StyleSpan;
import android.view.LayoutInflater;
import android.view.ViewGroup.LayoutParams;

import androidx.test.filters.SmallTest;

import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.params.BaseJUnit4RunnerDelegate;
import org.chromium.base.test.params.ParameterAnnotations.ClassParameter;
import org.chromium.base.test.params.ParameterAnnotations.UseRunnerDelegate;
import org.chromium.base.test.params.ParameterSet;
import org.chromium.base.test.params.ParameterizedRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Restriction;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;
import org.chromium.ui.test.util.BlankUiTestActivityTestCase;
import org.chromium.ui.test.util.NightModeTestUtils;
import org.chromium.ui.test.util.RenderTestRule;

import java.util.List;

/** Render tests for Message Banner. */
@RunWith(ParameterizedRunner.class)
@UseRunnerDelegate(BaseJUnit4RunnerDelegate.class)
@Batch(Batch.UNIT_TESTS)
public class MessageBannerRenderTest extends BlankUiTestActivityTestCase {
    @ClassParameter
    private static List<ParameterSet> sClassParams =
            new NightModeTestUtils.NightModeParams().getParameters();

    @Rule
    public RenderTestRule mRenderTestRule =
            RenderTestRule.Builder.withPublicCorpus()
                    .setBugComponent(RenderTestRule.Component.UI_BROWSER_MOBILE_MESSAGES)
                    .build();

    public MessageBannerRenderTest(boolean nightModeEnabled) {
        NightModeTestUtils.setUpNightModeForBlankUiTestActivity(nightModeEnabled);
        mRenderTestRule.setNightModeEnabled(nightModeEnabled);
    }

    @Test
    @SmallTest
    @Feature({"RenderTest", "Messages"})
    @Restriction({RESTRICTION_TYPE_NON_LOW_END_DEVICE})
    public void testBasic() throws Exception {
        Activity activity = getActivity();
        Drawable drawable =
                ApiCompatibilityUtils.getDrawable(
                        activity.getResources(), android.R.drawable.ic_delete);
        PropertyModel model =
                new PropertyModel.Builder(MessageBannerProperties.ALL_KEYS)
                        .with(
                                MessageBannerProperties.MESSAGE_IDENTIFIER,
                                MessageIdentifier.TEST_MESSAGE)
                        .with(MessageBannerProperties.ICON, drawable)
                        .with(MessageBannerProperties.TITLE, "Primary Title")
                        .with(MessageBannerProperties.DESCRIPTION, "Secondary Title")
                        .with(MessageBannerProperties.PRIMARY_BUTTON_TEXT, "Action")
                        .build();
        MessageBannerView view =
                (MessageBannerView)
                        LayoutInflater.from(activity)
                                .inflate(R.layout.message_banner_view, null, false);
        PropertyModelChangeProcessor.create(model, view, MessageBannerViewBinder::bind);
        LayoutParams params =
                new LayoutParams(
                        LayoutParams.MATCH_PARENT,
                        activity.getResources()
                                .getDimensionPixelSize(R.dimen.message_banner_height));

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    getActivity().setContentView(view, params);
                });
        mRenderTestRule.render(view, "message_banner_basic");
    }

    @Test
    @SmallTest
    @Feature({"RenderTest", "Messages"})
    @Restriction({RESTRICTION_TYPE_NON_LOW_END_DEVICE})
    public void testBasic_withSecondaryIcon() throws Exception {
        Activity activity = getActivity();
        Drawable drawable =
                ApiCompatibilityUtils.getDrawable(
                        activity.getResources(), android.R.drawable.ic_delete);
        Drawable drawable2 =
                ApiCompatibilityUtils.getDrawable(
                        activity.getResources(), android.R.drawable.ic_btn_speak_now);
        PropertyModel model =
                new PropertyModel.Builder(MessageBannerProperties.ALL_KEYS)
                        .with(
                                MessageBannerProperties.MESSAGE_IDENTIFIER,
                                MessageIdentifier.TEST_MESSAGE)
                        .with(MessageBannerProperties.ICON, drawable)
                        .with(MessageBannerProperties.TITLE, "Primary Title")
                        .with(MessageBannerProperties.DESCRIPTION, "Secondary Title")
                        .with(MessageBannerProperties.PRIMARY_BUTTON_TEXT, "Action")
                        .with(MessageBannerProperties.SECONDARY_ICON, drawable2)
                        .build();
        MessageBannerView view =
                (MessageBannerView)
                        LayoutInflater.from(activity)
                                .inflate(R.layout.message_banner_view, null, false);
        PropertyModelChangeProcessor.create(model, view, MessageBannerViewBinder::bind);
        LayoutParams params =
                new LayoutParams(
                        LayoutParams.MATCH_PARENT,
                        activity.getResources()
                                .getDimensionPixelSize(R.dimen.message_banner_height));

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    getActivity().setContentView(view, params);
                });
        mRenderTestRule.render(view, "message_banner_basic_with_secondary_icon");
    }

    @Test
    @SmallTest
    @Feature({"RenderTest", "Messages"})
    @Restriction({RESTRICTION_TYPE_NON_LOW_END_DEVICE})
    public void testBasic_withSpannableDescription() throws Exception {
        Activity activity = getActivity();
        Drawable drawable =
                ApiCompatibilityUtils.getDrawable(
                        activity.getResources(), android.R.drawable.ic_delete);
        SpannableString spannable = new SpannableString("Dummy Spannable Description!");
        StyleSpan boldSpan = new StyleSpan(Typeface.BOLD);
        spannable.setSpan(boldSpan, 0, spannable.length(), Spanned.SPAN_INCLUSIVE_EXCLUSIVE);
        PropertyModel model =
                new PropertyModel.Builder(MessageBannerProperties.ALL_KEYS)
                        .with(
                                MessageBannerProperties.MESSAGE_IDENTIFIER,
                                MessageIdentifier.TEST_MESSAGE)
                        .with(MessageBannerProperties.ICON, drawable)
                        .with(MessageBannerProperties.TITLE, "Primary Title")
                        .with(MessageBannerProperties.DESCRIPTION, spannable)
                        .with(MessageBannerProperties.PRIMARY_BUTTON_TEXT, "Action")
                        .build();
        MessageBannerView view =
                (MessageBannerView)
                        LayoutInflater.from(activity)
                                .inflate(R.layout.message_banner_view, null, false);
        PropertyModelChangeProcessor.create(model, view, MessageBannerViewBinder::bind);
        LayoutParams params =
                new LayoutParams(
                        LayoutParams.MATCH_PARENT,
                        activity.getResources()
                                .getDimensionPixelSize(R.dimen.message_banner_height));

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    getActivity().setContentView(view, params);
                });
        mRenderTestRule.render(view, "message_banner_basic_with_spannable_description");
    }

    @Test
    @SmallTest
    @Feature({"RenderTest", "Messages"})
    @Restriction({RESTRICTION_TYPE_NON_LOW_END_DEVICE})
    public void testBasic_multilineDescriptionMaxLines() throws Exception {
        Activity activity = getActivity();
        final String multilineDescription = "Line 1\nLine 2\nLine 3\nLine 4";
        PropertyModel model =
                new PropertyModel.Builder(MessageBannerProperties.ALL_KEYS)
                        .with(
                                MessageBannerProperties.MESSAGE_IDENTIFIER,
                                MessageIdentifier.TEST_MESSAGE)
                        .with(MessageBannerProperties.TITLE, "Primary Title")
                        .with(MessageBannerProperties.DESCRIPTION, multilineDescription)
                        .with(MessageBannerProperties.DESCRIPTION_MAX_LINES, 2)
                        .with(MessageBannerProperties.PRIMARY_BUTTON_TEXT, "Action")
                        .build();
        MessageBannerView view =
                (MessageBannerView)
                        LayoutInflater.from(activity)
                                .inflate(R.layout.message_banner_view, null, false);
        PropertyModelChangeProcessor.create(model, view, MessageBannerViewBinder::bind);
        LayoutParams params =
                new LayoutParams(LayoutParams.MATCH_PARENT, LayoutParams.WRAP_CONTENT);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    getActivity().setContentView(view, params);
                });
        mRenderTestRule.render(view, "message_banner_basic_with_multiline_description");
    }

    @Test
    @SmallTest
    @Feature({"RenderTest", "Messages"})
    @Restriction({RESTRICTION_TYPE_NON_LOW_END_DEVICE})
    public void testBasic_veryLongButtonText() throws Exception {
        Activity activity = getActivity();
        final String veryLongButtonText =
                "1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 81 19 20 21"
                        + " 22 23 24 25 26 27 28 29 30 31 32 33 34";
        PropertyModel model =
                new PropertyModel.Builder(MessageBannerProperties.ALL_KEYS)
                        .with(
                                MessageBannerProperties.MESSAGE_IDENTIFIER,
                                MessageIdentifier.TEST_MESSAGE)
                        .with(MessageBannerProperties.TITLE, "Primary Title")
                        .with(MessageBannerProperties.DESCRIPTION, "Secondary Title")
                        .with(MessageBannerProperties.DESCRIPTION_MAX_LINES, 2)
                        .with(MessageBannerProperties.PRIMARY_BUTTON_TEXT, veryLongButtonText)
                        .build();
        MessageBannerView view =
                (MessageBannerView)
                        LayoutInflater.from(activity)
                                .inflate(R.layout.message_banner_view, null, false);
        PropertyModelChangeProcessor.create(model, view, MessageBannerViewBinder::bind);
        LayoutParams params =
                new LayoutParams(LayoutParams.MATCH_PARENT, LayoutParams.WRAP_CONTENT);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    getActivity().setContentView(view, params);
                });
        mRenderTestRule.render(view, "message_banner_basic_with_very_long_button_text");
    }

    @Test
    @SmallTest
    @Feature({"RenderTest", "Messages"})
    @Restriction({RESTRICTION_TYPE_NON_LOW_END_DEVICE})
    public void testBasic_resetPrimaryButtonText() throws Exception {
        MessageBannerView result =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> {
                            Activity activity = getActivity();
                            final String veryLongButtonText =
                                    "1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 81 19 20 21"
                                            + " 22 23 24 25 26 27 28 29 30 31 32 33 34";
                            PropertyModel model =
                                    new PropertyModel.Builder(MessageBannerProperties.ALL_KEYS)
                                            .with(
                                                    MessageBannerProperties.MESSAGE_IDENTIFIER,
                                                    MessageIdentifier.TEST_MESSAGE)
                                            .with(MessageBannerProperties.TITLE, "Primary Title")
                                            .with(
                                                    MessageBannerProperties.DESCRIPTION,
                                                    "Secondary Title")
                                            .with(MessageBannerProperties.DESCRIPTION_MAX_LINES, 2)
                                            .with(
                                                    MessageBannerProperties.PRIMARY_BUTTON_TEXT,
                                                    veryLongButtonText)
                                            .build();
                            MessageBannerView view =
                                    (MessageBannerView)
                                            LayoutInflater.from(activity)
                                                    .inflate(
                                                            R.layout.message_banner_view,
                                                            null,
                                                            false);
                            PropertyModelChangeProcessor.create(
                                    model, view, MessageBannerViewBinder::bind);
                            LayoutParams params =
                                    new LayoutParams(
                                            LayoutParams.MATCH_PARENT, LayoutParams.WRAP_CONTENT);

                            getActivity().setContentView(view, params);
                            model.set(MessageBannerProperties.PRIMARY_BUTTON_TEXT, "Reset");
                            return view;
                        });
        mRenderTestRule.render(result, "message_banner_basic_with_reset_primary_button_text");
    }

    @Test
    @SmallTest
    @Feature({"RenderTest", "Messages"})
    @Restriction({RESTRICTION_TYPE_NON_LOW_END_DEVICE})
    public void testLayoutAfterClearingDescription() throws Exception {
        Activity activity = getActivity();
        Drawable drawable =
                ApiCompatibilityUtils.getDrawable(
                        activity.getResources(), android.R.drawable.ic_delete);
        PropertyModel model =
                new PropertyModel.Builder(MessageBannerProperties.ALL_KEYS)
                        .with(
                                MessageBannerProperties.MESSAGE_IDENTIFIER,
                                MessageIdentifier.TEST_MESSAGE)
                        .with(MessageBannerProperties.ICON, drawable)
                        .with(MessageBannerProperties.TITLE, "Primary Title")
                        .with(MessageBannerProperties.DESCRIPTION, "Secondary Title")
                        .with(MessageBannerProperties.PRIMARY_BUTTON_TEXT, "Action")
                        .build();
        MessageBannerView view =
                (MessageBannerView)
                        LayoutInflater.from(activity)
                                .inflate(R.layout.message_banner_view, null, false);
        PropertyModelChangeProcessor.create(model, view, MessageBannerViewBinder::bind);
        LayoutParams params =
                new LayoutParams(
                        LayoutParams.MATCH_PARENT,
                        activity.getResources()
                                .getDimensionPixelSize(R.dimen.message_banner_height));

        model.set(MessageBannerProperties.DESCRIPTION, null);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    getActivity().setContentView(view, params);
                });
        mRenderTestRule.render(view, "message_banner_layout_after_clearing_description");
    }

    @Test
    @SmallTest
    @Feature({"RenderTest", "Messages"})
    @Restriction({RESTRICTION_TYPE_NON_LOW_END_DEVICE})
    public void testLargeIcon() throws Exception {
        Activity activity = getActivity();
        Drawable drawable =
                ApiCompatibilityUtils.getDrawable(
                        activity.getResources(), android.R.drawable.ic_delete);
        PropertyModel model =
                new PropertyModel.Builder(MessageBannerProperties.ALL_KEYS)
                        .with(
                                MessageBannerProperties.MESSAGE_IDENTIFIER,
                                MessageIdentifier.TEST_MESSAGE)
                        .with(MessageBannerProperties.ICON, drawable)
                        .with(MessageBannerProperties.TITLE, "Primary Title")
                        .with(MessageBannerProperties.DESCRIPTION, "Secondary Title")
                        .with(MessageBannerProperties.PRIMARY_BUTTON_TEXT, "Action")
                        .with(MessageBannerProperties.LARGE_ICON, true)
                        .build();
        MessageBannerView view =
                (MessageBannerView)
                        LayoutInflater.from(activity)
                                .inflate(R.layout.message_banner_view, null, false);
        PropertyModelChangeProcessor.create(model, view, MessageBannerViewBinder::bind);
        LayoutParams params =
                new LayoutParams(
                        LayoutParams.MATCH_PARENT,
                        activity.getResources()
                                .getDimensionPixelSize(R.dimen.message_banner_height));

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    getActivity().setContentView(view, params);
                });
        mRenderTestRule.render(view, "message_banner_large_icon");
    }

    @Test
    @SmallTest
    @Feature({"RenderTest", "Messages"})
    @Restriction({RESTRICTION_TYPE_NON_LOW_END_DEVICE})
    public void testLargeIconWithRadius() throws Exception {
        Activity activity = getActivity();
        Bitmap.Config conf = Bitmap.Config.ARGB_8888;
        int w = activity.getResources().getDimensionPixelSize(R.dimen.message_icon_size_large);
        Bitmap bmp = Bitmap.createBitmap(w, w, conf);
        bmp.eraseColor(Color.RED);
        BitmapDrawable drawable = new BitmapDrawable(bmp);
        int radius = activity.getResources().getDimensionPixelSize(R.dimen.message_icon_size) / 2;
        PropertyModel model =
                new PropertyModel.Builder(MessageBannerProperties.ALL_KEYS)
                        .with(
                                MessageBannerProperties.MESSAGE_IDENTIFIER,
                                MessageIdentifier.TEST_MESSAGE)
                        .with(MessageBannerProperties.ICON, drawable)
                        .with(MessageBannerProperties.TITLE, "Primary Title")
                        .with(MessageBannerProperties.DESCRIPTION, "Secondary Title")
                        .with(MessageBannerProperties.PRIMARY_BUTTON_TEXT, "Action")
                        .with(MessageBannerProperties.LARGE_ICON, true)
                        .with(MessageBannerProperties.ICON_ROUNDED_CORNER_RADIUS_PX, radius)
                        .build();
        MessageBannerView view =
                (MessageBannerView)
                        LayoutInflater.from(activity)
                                .inflate(R.layout.message_banner_view, null, false);
        PropertyModelChangeProcessor.create(model, view, MessageBannerViewBinder::bind);
        LayoutParams params =
                new LayoutParams(
                        LayoutParams.MATCH_PARENT,
                        activity.getResources()
                                .getDimensionPixelSize(R.dimen.message_banner_height));

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    getActivity().setContentView(view, params);
                });
        mRenderTestRule.render(view, "message_banner_large_icon_with_radius");
    }

    @Test
    @SmallTest
    @Feature({"RenderTest", "Messages"})
    @Restriction({RESTRICTION_TYPE_NON_LOW_END_DEVICE})
    public void testDescriptionIconWithDefaultSize() throws Exception {
        Activity activity = getActivity();
        Drawable messageIcon =
                ApiCompatibilityUtils.getDrawable(
                        activity.getResources(), android.R.drawable.ic_delete);
        Drawable descriptionIcon =
                ApiCompatibilityUtils.getDrawable(
                        activity.getResources(), R.drawable.ic_photo_camera_black);
        PropertyModel model =
                new PropertyModel.Builder(MessageBannerProperties.ALL_KEYS)
                        .with(
                                MessageBannerProperties.MESSAGE_IDENTIFIER,
                                MessageIdentifier.TEST_MESSAGE)
                        .with(MessageBannerProperties.ICON, messageIcon)
                        .with(MessageBannerProperties.TITLE, "Primary Title")
                        .with(MessageBannerProperties.DESCRIPTION_ICON, descriptionIcon)
                        .with(MessageBannerProperties.PRIMARY_BUTTON_TEXT, "Action")
                        .build();
        MessageBannerView view =
                (MessageBannerView)
                        LayoutInflater.from(activity)
                                .inflate(R.layout.message_banner_view, null, false);
        PropertyModelChangeProcessor.create(model, view, MessageBannerViewBinder::bind);
        LayoutParams params =
                new LayoutParams(
                        LayoutParams.MATCH_PARENT,
                        activity.getResources()
                                .getDimensionPixelSize(R.dimen.message_banner_height));

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    getActivity().setContentView(view, params);
                });
        mRenderTestRule.render(view, "message_banner_description_icon_with_default_size");
    }

    @Test
    @SmallTest
    @Feature({"RenderTest", "Messages"})
    @Restriction({RESTRICTION_TYPE_NON_LOW_END_DEVICE})
    public void testDescriptionIconWithResizing() throws Exception {
        Activity activity = getActivity();
        Drawable messageIcon =
                ApiCompatibilityUtils.getDrawable(
                        activity.getResources(), android.R.drawable.ic_delete);
        Drawable descriptionIcon =
                ApiCompatibilityUtils.getDrawable(
                        activity.getResources(), R.drawable.ic_photo_camera_black);
        PropertyModel model =
                new PropertyModel.Builder(MessageBannerProperties.ALL_KEYS)
                        .with(
                                MessageBannerProperties.MESSAGE_IDENTIFIER,
                                MessageIdentifier.TEST_MESSAGE)
                        .with(MessageBannerProperties.ICON, messageIcon)
                        .with(MessageBannerProperties.TITLE, "Primary Title")
                        .with(MessageBannerProperties.DESCRIPTION_ICON, descriptionIcon)
                        .with(MessageBannerProperties.RESIZE_DESCRIPTION_ICON, true)
                        .with(MessageBannerProperties.PRIMARY_BUTTON_TEXT, "Action")
                        .build();
        MessageBannerView view =
                (MessageBannerView)
                        LayoutInflater.from(activity)
                                .inflate(R.layout.message_banner_view, null, false);
        PropertyModelChangeProcessor.create(model, view, MessageBannerViewBinder::bind);
        LayoutParams params =
                new LayoutParams(
                        LayoutParams.MATCH_PARENT,
                        activity.getResources()
                                .getDimensionPixelSize(R.dimen.message_banner_height));

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    getActivity().setContentView(view, params);
                });
        mRenderTestRule.render(view, "message_banner_description_icon_with_resizing");
    }

    @Test
    @SmallTest
    @Feature({"RenderTest", "Messages"})
    @Restriction({RESTRICTION_TYPE_NON_LOW_END_DEVICE})
    public void testDescriptionIconWithText() throws Exception {
        Activity activity = getActivity();
        Drawable messageIcon =
                ApiCompatibilityUtils.getDrawable(
                        activity.getResources(), android.R.drawable.ic_delete);
        Drawable descriptionIcon =
                ApiCompatibilityUtils.getDrawable(
                        activity.getResources(), R.drawable.ic_photo_camera_black);
        PropertyModel model =
                new PropertyModel.Builder(MessageBannerProperties.ALL_KEYS)
                        .with(
                                MessageBannerProperties.MESSAGE_IDENTIFIER,
                                MessageIdentifier.TEST_MESSAGE)
                        .with(MessageBannerProperties.ICON, messageIcon)
                        .with(MessageBannerProperties.TITLE, "Primary Title")
                        .with(MessageBannerProperties.DESCRIPTION_ICON, descriptionIcon)
                        .with(MessageBannerProperties.RESIZE_DESCRIPTION_ICON, true)
                        .with(MessageBannerProperties.DESCRIPTION, "Secondary Title")
                        .with(MessageBannerProperties.PRIMARY_BUTTON_TEXT, "Action")
                        .build();
        MessageBannerView view =
                (MessageBannerView)
                        LayoutInflater.from(activity)
                                .inflate(R.layout.message_banner_view, null, false);
        PropertyModelChangeProcessor.create(model, view, MessageBannerViewBinder::bind);
        LayoutParams params =
                new LayoutParams(
                        LayoutParams.MATCH_PARENT,
                        activity.getResources()
                                .getDimensionPixelSize(R.dimen.message_banner_height));

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    getActivity().setContentView(view, params);
                });
        mRenderTestRule.render(view, "message_banner_description_icon_with_text");
    }

    @Test
    @SmallTest
    @Feature({"RenderTest", "Messages"})
    @Restriction({RESTRICTION_TYPE_LOW_END_DEVICE})
    public void testBasic_lowEnd() throws Exception {
        Activity activity = getActivity();
        Drawable drawable =
                ApiCompatibilityUtils.getDrawable(
                        activity.getResources(), android.R.drawable.ic_delete);
        PropertyModel model =
                new PropertyModel.Builder(MessageBannerProperties.ALL_KEYS)
                        .with(
                                MessageBannerProperties.MESSAGE_IDENTIFIER,
                                MessageIdentifier.TEST_MESSAGE)
                        .with(MessageBannerProperties.ICON, drawable)
                        .with(MessageBannerProperties.TITLE, "Primary Title")
                        .with(MessageBannerProperties.DESCRIPTION, "Secondary Title")
                        .with(MessageBannerProperties.PRIMARY_BUTTON_TEXT, "Action")
                        .build();
        MessageBannerView view =
                (MessageBannerView)
                        LayoutInflater.from(activity)
                                .inflate(R.layout.message_banner_view, null, false);
        PropertyModelChangeProcessor.create(model, view, MessageBannerViewBinder::bind);
        LayoutParams params =
                new LayoutParams(
                        LayoutParams.MATCH_PARENT,
                        activity.getResources()
                                .getDimensionPixelSize(R.dimen.message_banner_height));

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    getActivity().setContentView(view, params);
                });
        mRenderTestRule.render(view, "message_banner_basic_low_end");
    }
}
