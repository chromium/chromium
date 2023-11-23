// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.subresource_filter;

import android.text.SpannableString;
import android.text.SpannableStringBuilder;
import android.text.Spanned;
import android.view.View;
import android.widget.CompoundButton;
import android.widget.CompoundButton.OnCheckedChangeListener;

import androidx.appcompat.widget.SwitchCompat;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;

import org.chromium.chrome.browser.infobar.ActionType;
import org.chromium.components.infobars.ConfirmInfoBar;
import org.chromium.components.infobars.InfoBar;
import org.chromium.components.infobars.InfoBarControlLayout;
import org.chromium.components.infobars.InfoBarLayout;
import org.chromium.ui.text.NoUnderlineClickableSpan;
import org.chromium.ui.widget.ButtonCompat;

/**
 * This infobar appears when ads are being blocked on the page. This occurs after proceeding through
 * an interstitial warning that the site shows deceptive content, or when the site is known to show
 * intrusive ads.
 */
@JNINamespace("subresource_filter")
public class AdsBlockedInfoBar extends ConfirmInfoBar implements OnCheckedChangeListener {
    private final String mMessage;
    private final String mFollowUpMessage;
    private final String mOKButtonText;
    private final String mReloadButtonText;
    private final String mToggleText;
    private boolean mIsShowingExplanation;
    private boolean mReloadIsToggled;
    private ButtonCompat mButton;

    @CalledByNative
    private static InfoBar show(
            int iconId,
            String message,
            String oKButtonText,
            String reloadButtonText,
            String toggleText,
            String followUpMessage) {
        return new AdsBlockedInfoBar(
                iconId, message, oKButtonText, reloadButtonText, toggleText, followUpMessage);
    }

    private AdsBlockedInfoBar(
            int iconDrawbleId,
            String message,
            String oKButtonText,
            String reloadButtonText,
            String toggleText,
            String followUpMessage) {
        super(iconDrawbleId, R.color.infobar_icon_drawable_color, null, message, null, null, null);
        mFollowUpMessage = followUpMessage;
        mMessage = message;
        mOKButtonText = oKButtonText;
        mReloadButtonText = reloadButtonText;
        mToggleText = toggleText;
    }

    @Override
    public void createContent(InfoBarLayout layout) {
        super.createContent(layout);
        if (mIsShowingExplanation) {
            String title = layout.getContext().getString(R.string.blocked_ads_prompt_title);
            layout.setMessage(title);

            SpannableStringBuilder description = new SpannableStringBuilder();
            description.append(new SpannableString(mFollowUpMessage));
            description.append(" ");
            int spanStart = description.length();
            String learnMore = layout.getContext().getString(R.string.learn_more);
            description.append(learnMore);

            NoUnderlineClickableSpan clickableSpan =
                    new NoUnderlineClickableSpan(layout.getContext(), (view) -> onLinkClicked());
            description.setSpan(
                    clickableSpan,
                    spanStart,
                    description.length(),
                    Spanned.SPAN_EXCLUSIVE_EXCLUSIVE);
            layout.getMessageLayout().addDescription(description);

            setButtons(layout, mOKButtonText, null);
            InfoBarControlLayout controlLayout = layout.addControlLayout();

            // Add a toggle button and ensure the button text is changed when the toggle changes.
            View switchView =
                    controlLayout.addSwitch(
                            0, 0, mToggleText, R.id.subresource_filter_infobar_toggle, false);
            SwitchCompat toggle =
                    (SwitchCompat) switchView.findViewById(R.id.subresource_filter_infobar_toggle);
            toggle.setOnCheckedChangeListener(this);
            mButton = layout.getPrimaryButton();

            // Ensure that the button does not resize when switching text.
            // TODO(csharrison,dfalcantara): setMinEms is wrong. Code should measure both pieces of
            // text and set the min width using those measurements. See crbug.com/708719.
            mButton.setMinEms(Math.max(mOKButtonText.length(), mReloadButtonText.length()));
        } else {
            String link = layout.getContext().getString(R.string.details_link);
            layout.setMessage(mMessage);
            layout.appendMessageLinkText(link);
        }
    }

    @Override
    public void onLinkClicked() {
        // If we aren't already showing the explanation, clicking the link should expand to show the
        // explanation. If we *are* already showing the explanation, clicking the link (which should
        // change to Learn more) should take us to the help page.
        if (!mIsShowingExplanation) {
            mIsShowingExplanation = true;
            replaceView(createView());
        }
        super.onLinkClicked();
    }

    @Override
    public void onButtonClicked(final boolean isPrimaryButton) {
        onButtonClicked(mReloadIsToggled ? ActionType.CANCEL : ActionType.OK);
    }

    @Override
    public void onCheckedChanged(CompoundButton buttonView, boolean isChecked) {
        assert mButton != null;
        mButton.setText(isChecked ? mReloadButtonText : mOKButtonText);
        mReloadIsToggled = isChecked;
    }
}
