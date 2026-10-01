// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.enterprise_signals_disclaimer;

import android.content.Context;

import androidx.annotation.StringRes;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.signin.services.BadgeConfig;
import org.chromium.chrome.browser.signin.services.DisplayableProfileData;
import org.chromium.chrome.browser.signin.services.ProfileDataCache;
import org.chromium.chrome.browser.ui.enterprise_signals_disclaimer.EnterpriseSignalsDisclaimerCoordinator.PresentationMode;
import org.chromium.components.signin.base.CoreAccountInfo;
import org.chromium.components.signin.identitymanager.IdentityManager;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.text.ChromeClickableSpan;
import org.chromium.ui.text.SpanApplier;
import org.chromium.ui.text.SpanApplier.SpanInfo;

/**
 * Mediator for the enterprise signals disclaimer.
 *
 * <p>This disclaimer is shown on browser startup to managed users missing the consent for
 * collecting the device signals. The dialog offers two choices: Accept or Decline. Accepting the
 * dialog dismisses it and allows the user to proceed with using the browser while also marking the
 * consent as granted. Dismissing the dialog with a gesture or explicitly clicking 'Sign out' will
 * sign the user out.
 */
@NullMarked
class EnterpriseSignalsDisclaimerMediator implements ProfileDataCache.Observer {
    // TODO(b/537182192): Replace with a p-link.
    static final String LEARN_MORE_LINK = "https://support.google.com/chrome/a/answer/16191236";

    /** Delegate for the enterprise signals disclaimer mediator. */
    public interface Delegate {
        /**
         * Opens the info page for the given URL in CTT.
         *
         * @param url The URL of the webpage to show.
         */
        void showInfoPage(String url);

        /** Called when the user taps the accept button. */
        void onAccept();

        /** Called when the user taps the decline button. */
        void onDecline();
    }

    private final PropertyModel mModel;
    private final ProfileDataCache mProfileDataCache;
    private final CoreAccountInfo mAccount;
    private final Delegate mDelegate;
    private boolean mIsDecisionHandled;

    /**
     * @param context The Android {@link Context}.
     * @param identityManager The {@link IdentityManager} used to back the {@link ProfileDataCache}.
     * @param account The account the disclaimer is shown for. This account is not required to be
     *     signed in yet.
     * @param presentationMode How the embedder is going to present the disclaimer.
     * @param delegate The {@link Delegate} handling the user's decision.
     */
    EnterpriseSignalsDisclaimerMediator(
            Context context,
            IdentityManager identityManager,
            CoreAccountInfo account,
            @PresentationMode int presentationMode,
            EnterpriseSignalsDisclaimerMediator.Delegate delegate) {
        mDelegate = delegate;
        mAccount = account;
        boolean isFre = presentationMode == PresentationMode.FIRST_RUN_EXPERIENCE;

        // Puts the badge in the bottom right corner of the profile picture.
        BadgeConfig badgeConfig =
                BadgeConfig.create(R.drawable.enterprise_badge_icon)
                        .withBadgeSize(R.dimen.enterprise_signals_disclaimer_badge_size)
                        .withBorderSize(R.dimen.enterprise_signals_disclaimer_badge_border_size)
                        .withXPosition(R.dimen.enterprise_signals_disclaimer_badge_x_position)
                        .withYPosition(R.dimen.enterprise_signals_disclaimer_badge_y_position)
                        .build(context);
        mProfileDataCache =
                ProfileDataCache.createWithoutBadge(
                        context,
                        identityManager,
                        R.dimen.enterprise_signals_disclaimer_profile_picture_size);
        mProfileDataCache.setBadge(badgeConfig);

        // The FRE uses dedicated strings, and moves the "Learn more" link from the description
        // into a footer below the buttons.
        @StringRes
        int titleId =
                isFre
                        ? R.string.enterprise_signals_disclaimer_fre_title
                        : R.string.enterprise_signals_disclaimer_title;
        CharSequence description =
                isFre
                        ? context.getString(R.string.enterprise_signals_disclaimer_fre_description)
                        : getTextWithLearnMoreLink(
                                context, R.string.enterprise_signals_disclaimer_description);
        @StringRes
        int acceptButtonTextId =
                isFre
                        ? R.string.enterprise_signals_disclaimer_fre_accept_button_text
                        : R.string.enterprise_signals_disclaimer_accept_button_text;
        @StringRes
        int cancelButtonTextId =
                isFre
                        ? R.string.enterprise_signals_disclaimer_fre_cancel_button_text
                        : R.string.enterprise_signals_disclaimer_cancel_button_text;

        PropertyModel.Builder builder =
                new PropertyModel.Builder(EnterpriseSignalsDisclaimerProperties.ALL_KEYS)
                        .with(
                                EnterpriseSignalsDisclaimerProperties.PROFILE_PICTURE,
                                mProfileDataCache.getById(mAccount.getId()).getImage())
                        .with(
                                EnterpriseSignalsDisclaimerProperties.TITLE,
                                context.getString(titleId))
                        .with(EnterpriseSignalsDisclaimerProperties.DESCRIPTION, description)
                        .with(
                                EnterpriseSignalsDisclaimerProperties.PROFILE_INFORMATION_TITLE,
                                context.getString(
                                        R.string
                                                .enterprise_signals_disclaimer_profile_information_title))
                        .with(
                                EnterpriseSignalsDisclaimerProperties.PROFILE_INFORMATION_DETAILS,
                                context.getString(
                                        R.string
                                                .enterprise_signals_disclaimer_profile_information_details))
                        .with(
                                EnterpriseSignalsDisclaimerProperties.DEVICE_INFORMATION_TITLE,
                                context.getString(
                                        R.string
                                                .enterprise_signals_disclaimer_device_information_title))
                        .with(
                                EnterpriseSignalsDisclaimerProperties.DEVICE_INFORMATION_DETAILS,
                                context.getString(
                                        R.string
                                                .enterprise_signals_disclaimer_device_information_details))
                        .with(
                                EnterpriseSignalsDisclaimerProperties.ACCEPT_BUTTON_TEXT,
                                context.getString(acceptButtonTextId))
                        .with(
                                EnterpriseSignalsDisclaimerProperties.CANCEL_BUTTON_TEXT,
                                context.getString(cancelButtonTextId))
                        .with(
                                EnterpriseSignalsDisclaimerProperties.ON_ACCEPT_CLICKED,
                                v -> onAcceptButtonClicked())
                        .with(
                                EnterpriseSignalsDisclaimerProperties.ON_CANCEL_CLICKED,
                                v -> onCancelButtonClicked());
        if (isFre) {
            builder.with(
                    EnterpriseSignalsDisclaimerProperties.FOOTER,
                    getTextWithLearnMoreLink(
                            context, R.string.enterprise_signals_disclaimer_fre_footer));
        }
        mModel = builder.build();

        mProfileDataCache.addObserver(this);
    }

    /**
     * Returns the PropertyModel managed by this mediator.
     *
     * @return The PropertyModel.
     */
    public PropertyModel getModel() {
        return mModel;
    }

    /** Dismisses the dialog and marks the device signals collection consent as granted. */
    private void onAcceptButtonClicked() {
        if (mIsDecisionHandled) return;
        mIsDecisionHandled = true;
        mDelegate.onAccept();
    }

    /** Called when the user explicitly clicks the Cancel button in the UI. */
    private void onCancelButtonClicked() {
        if (mIsDecisionHandled) return;
        mIsDecisionHandled = true;
        mDelegate.onDecline();
    }

    /** Implements {@link ProfileDataCache.Observer}. */
    @Override
    public void onProfileDataUpdated(DisplayableProfileData profileData) {
        if (profileData.getAccountId().equals(mAccount.getId())) {
            mModel.set(
                    EnterpriseSignalsDisclaimerProperties.PROFILE_PICTURE, profileData.getImage());
        }
    }

    void destroy() {
        mProfileDataCache.removeObserver(this);
    }

    private CharSequence getTextWithLearnMoreLink(Context context, @StringRes int stringId) {
        final ChromeClickableSpan learnMoreSpan =
                new ChromeClickableSpan(context, v -> mDelegate.showInfoPage(LEARN_MORE_LINK));
        return SpanApplier.applySpans(
                context.getString(stringId), new SpanInfo("<LINK>", "</LINK>", learnMoreSpan));
    }
}
