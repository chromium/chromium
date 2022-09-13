// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.autofill_assistant.onboarding;

import android.content.Context;
import android.view.View;
import android.widget.ScrollView;
import android.widget.TextView;

import androidx.annotation.Nullable;

import org.chromium.components.autofill_assistant.AssistantBrowserControlsFactory;
import org.chromium.components.autofill_assistant.AssistantInfoPageUtil;
import org.chromium.components.autofill_assistant.R;
import org.chromium.components.autofill_assistant.generic_ui.AssistantDimension;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.widget.scrim.ScrimCoordinator;
import org.chromium.components.browser_ui.widget.textbubble.TextBubble;
import org.chromium.content_public.browser.BrowserContextHandle;
import org.chromium.ui.util.AccessibilityUtil;
import org.chromium.ui.widget.ViewRectProvider;

import java.util.Map;

/**
 * Similar to BottomSheetOnboardingWithPopupCoordinator, but this experimental onboarding
 * coordinator shows the subtitle in a bubble popup instead of in the bottom sheet.
 */
public class BottomSheetOnboardingWithPopupAndBubbleCoordinator
        extends BottomSheetOnboardingWithPopupCoordinator {
    /** The amount of space to put between the top of the sheet and the bottom of the bubble.*/
    private static final int TEXT_BUBBLE_PIXELS_ABOVE_SHEET = 4;
    /**
     * The vertical offset to be applied to the text bubble. The intent is for the arrow of the
     * bubble to point at the edge of the bottom sheet. The value represents the sum of spaces and
     * margins measured from the top of the sheet to the top of the poodle (where the text bubble is
     * anchored).
     */
    private static final int TEXT_BUBBLE_VERTICAL_OFFSET_DP = 34;

    @Nullable
    TextBubble mTextBubble;

    BottomSheetOnboardingWithPopupAndBubbleCoordinator(BrowserContextHandle browserContext,
            AssistantInfoPageUtil infoPageUtil, String experimentIds,
            Map<String, String> parameters, Context context, BottomSheetController controller,
            AssistantBrowserControlsFactory browserControlsFactory, View rootView,
            ScrimCoordinator scrim, AccessibilityUtil accessibilityUtil) {
        super(browserContext, infoPageUtil, experimentIds, parameters, context, controller,
                browserControlsFactory, rootView, scrim, accessibilityUtil);
    }

    @Override
    ScrollView createViewImpl() {
        // The subtitle will instead be shown in a text bubble.
        ScrollView view = super.createViewImpl();
        view.findViewById(R.id.onboarding_subtitle_container).setVisibility(View.GONE);
        return view;
    }

    @Override
    void showViewImpl() {
        super.showViewImpl();

        // Extract the subtitle text from the (hidden) view and show it in the text bubble instead.
        // We can't just this from our resources since string replacements may have been performed.
        TextView subtitleView = mView.findViewById(R.id.onboarding_subtitle);
        String subtitleText = subtitleView.getText().toString();

        View poodleView = mView.findViewById(R.id.onboarding_title_poodle);
        ViewRectProvider anchorRectProvider = new ViewRectProvider(poodleView);
        // Offset the text bubble such that it points to the top border of the bottom sheet.
        int topOffset = AssistantDimension.getPixelSizeDp(mContext, TEXT_BUBBLE_VERTICAL_OFFSET_DP)
                + TEXT_BUBBLE_PIXELS_ABOVE_SHEET;
        anchorRectProvider.setInsetPx(0, -topOffset, 0, 0);
        mTextBubble = new TextBubble(
                /* context = */ mContext, /* rootView = */ poodleView,
                /* contentString = */ subtitleText,
                /* accessibilityString = */ subtitleText, /*showArrow = */ true,
                /* anchorRectProvider = */ anchorRectProvider,
                mAccessibilityUtil.isAccessibilityEnabled());
        mTextBubble.setDismissOnTouchInteraction(true);
        mTextBubble.show();
    }

    @Override
    public void hide() {
        if (mTextBubble != null) {
            mTextBubble.dismiss();
            mTextBubble = null;
        }
        super.hide();
    }
}
