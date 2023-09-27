// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.android.webid;

import android.content.Context;
import android.content.res.ColorStateList;
import android.content.res.Resources;
import android.graphics.Bitmap;
import android.graphics.Bitmap.Config;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Paint;
import android.graphics.PorterDuff.Mode;
import android.graphics.PorterDuffXfermode;
import android.graphics.Rect;
import android.graphics.drawable.BitmapDrawable;
import android.graphics.drawable.Drawable;
import android.text.SpannableString;
import android.text.method.LinkMovementMethod;
import android.view.View;
import android.widget.ImageView;
import android.widget.TextView;

import androidx.annotation.StringRes;

import com.google.android.material.color.MaterialColors;

import org.chromium.base.Callback;
import org.chromium.chrome.browser.customtabs.CustomTabActivity;
import org.chromium.chrome.browser.ui.android.webid.AccountSelectionProperties.AccountProperties;
import org.chromium.chrome.browser.ui.android.webid.AccountSelectionProperties.ContinueButtonProperties;
import org.chromium.chrome.browser.ui.android.webid.AccountSelectionProperties.DataSharingConsentProperties;
import org.chromium.chrome.browser.ui.android.webid.AccountSelectionProperties.ErrorButtonProperties;
import org.chromium.chrome.browser.ui.android.webid.AccountSelectionProperties.ErrorProperties;
import org.chromium.chrome.browser.ui.android.webid.AccountSelectionProperties.HeaderProperties;
import org.chromium.chrome.browser.ui.android.webid.AccountSelectionProperties.IdpSignInProperties;
import org.chromium.chrome.browser.ui.android.webid.AccountSelectionProperties.ItemProperties;
import org.chromium.chrome.browser.ui.android.webid.data.Account;
import org.chromium.chrome.browser.ui.android.webid.data.IdentityProviderMetadata;
import org.chromium.components.browser_ui.util.AvatarGenerator;
import org.chromium.components.browser_ui.widget.RoundedIconGenerator;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor.ViewBinder;
import org.chromium.ui.text.NoUnderlineClickableSpan;
import org.chromium.ui.text.SpanApplier;
import org.chromium.ui.util.ColorUtils;
import org.chromium.ui.widget.ButtonCompat;
import org.chromium.url.GURL;

import java.util.ArrayList;
import java.util.List;

/**
 * Provides functions that map {@link AccountSelectionProperties} changes in a {@link PropertyModel}
 * to the suitable method in {@link AccountSelectionView}.
 */
class AccountSelectionViewBinder {
    private static final String TAG = "AccountSelectionView";

    /**
     * Returns bitmap with the maskable bitmap's safe zone as defined in
     * https://www.w3.org/TR/appmanifest/ cropped in a circle.
     * @param resources the Resources used to set initial target density.
     * @param bitmap the maskable bitmap. It should adhere to the maskable icon spec as defined in
     * https://www.w3.org/TR/appmanifest/
     * @param outBitmapSize the target bitmap size in pixels.
     * @return the cropped bitmap.
     */
    public static Drawable createBitmapWithMaskableIconSafeZone(
            Resources resources, Bitmap bitmap, int outBitmapSize) {
        int cropWidth = (int) Math.floor(
                bitmap.getWidth() * AccountSelectionBridge.MASKABLE_ICON_SAFE_ZONE_DIAMETER_RATIO);
        int cropHeight = (int) Math.floor(
                bitmap.getHeight() * AccountSelectionBridge.MASKABLE_ICON_SAFE_ZONE_DIAMETER_RATIO);
        int cropX = (int) Math.floor((bitmap.getWidth() - cropWidth) / 2.0f);
        int cropY = (int) Math.floor((bitmap.getHeight() - cropHeight) / 2.0f);

        Bitmap output = Bitmap.createBitmap(outBitmapSize, outBitmapSize, Config.ARGB_8888);
        Canvas canvas = new Canvas(output);
        // Fill the canvas with transparent color.
        canvas.drawColor(Color.TRANSPARENT);
        // Draw a white circle.
        float radius = (float) outBitmapSize / 2;
        Paint paint = new Paint();
        paint.setAntiAlias(true);
        paint.setColor(Color.WHITE);
        canvas.drawCircle(radius, radius, radius, paint);
        // Use SRC_IN so white circle acts as a mask while drawing the avatar.
        paint.setXfermode(new PorterDuffXfermode(Mode.SRC_IN));
        canvas.drawBitmap(bitmap, new Rect(cropX, cropY, cropWidth + cropX, cropHeight + cropY),
                new Rect(0, 0, outBitmapSize, outBitmapSize), paint);
        return new BitmapDrawable(resources, output);
    }

    /**
     * Called whenever an account is bound to this view.
     * @param model The model containing the data for the view.
     * @param view The view to be bound.
     * @param key The key of the property to be bound.
     */
    static void bindAccountView(PropertyModel model, View view, PropertyKey key) {
        Account account = model.get(AccountProperties.ACCOUNT);
        if (key == AccountProperties.AVATAR) {
            AccountProperties.Avatar avatarData = model.get(AccountProperties.AVATAR);
            int avatarSize = avatarData.mAvatarSize;
            Bitmap avatar = avatarData.mAvatar;

            Resources resources = view.getContext().getResources();

            // Prepare avatar or its fallback monogram.
            if (avatar == null) {
                int avatarMonogramTextSize = view.getResources().getDimensionPixelSize(
                        R.dimen.account_selection_account_avatar_monogram_text_size);
                // TODO(crbug.com/1295017): Consult UI team to determine the background color we
                // need to use here.
                RoundedIconGenerator roundedIconGenerator =
                        new RoundedIconGenerator(resources, avatarSize /* iconWidthDp */,
                                avatarSize /* iconHeightDp */, avatarSize / 2 /* cornerRadiusDp */,
                                Color.GRAY /* backgroundColor */, avatarMonogramTextSize);
                avatar = roundedIconGenerator.generateIconForText(avatarData.mName);
            }
            Drawable croppedAvatar = AvatarGenerator.makeRoundAvatar(resources, avatar, avatarSize);

            ImageView avatarView = view.findViewById(R.id.start_icon);
            avatarView.setImageDrawable(croppedAvatar);
        } else if (key == AccountProperties.ON_CLICK_LISTENER) {
            Callback<Account> clickCallback = model.get(AccountProperties.ON_CLICK_LISTENER);
            if (clickCallback == null) {
                view.setOnClickListener(null);
            } else {
                view.setOnClickListener(clickedView -> { clickCallback.onResult(account); });
            }
        } else if (key == AccountProperties.ACCOUNT) {
            TextView subject = view.findViewById(R.id.title);
            subject.setText(account.getName());
            TextView email = view.findViewById(R.id.description);
            email.setText(account.getEmail());
        } else {
            assert false : "Unhandled update to property:" + key;
        }
    }

    static SpanApplier.SpanInfo createLink(
            Context context, String tag, GURL url, Runnable clickRunnable) {
        if (GURL.isEmptyOrInvalid(url)) return null;

        String startTag = "<" + tag + ">";
        String endTag = "</" + tag + ">";
        Callback<View> onClickCallback = v -> {
            CustomTabActivity.showInfoPage(context, url.getSpec());
            clickRunnable.run();
        };
        return new SpanApplier.SpanInfo(
                startTag, endTag, new NoUnderlineClickableSpan(context, onClickCallback));
    }

    /**
     * Called whenever a user data sharing consent is bound to this view.
     * @param model The model containing the data for the view.
     * @param view The view to be bound.
     * @param key The key of the property to be bound.
     */
    static void bindDataSharingConsentView(PropertyModel model, View view, PropertyKey key) {
        if (key == DataSharingConsentProperties.PROPERTIES) {
            DataSharingConsentProperties.Properties properties =
                    model.get(DataSharingConsentProperties.PROPERTIES);

            Context context = view.getContext();
            SpanApplier.SpanInfo privacyPolicySpan = createLink(context, "link_privacy_policy",
                    properties.mPrivacyPolicyUrl, properties.mPrivacyPolicyClickRunnable);
            SpanApplier.SpanInfo termsOfServiceSpan = createLink(context, "link_terms_of_service",
                    properties.mTermsOfServiceUrl, properties.mTermsOfServiceClickRunnable);

            int consentTextId;
            if (privacyPolicySpan == null && termsOfServiceSpan == null) {
                consentTextId = R.string.account_selection_data_sharing_consent_no_pp_or_tos;
            } else if (privacyPolicySpan == null) {
                consentTextId = R.string.account_selection_data_sharing_consent_no_pp;
            } else if (termsOfServiceSpan == null) {
                consentTextId = R.string.account_selection_data_sharing_consent_no_tos;
            } else {
                consentTextId = R.string.account_selection_data_sharing_consent;
            }
            String consentText =
                    String.format(context.getString(consentTextId), properties.mIdpForDisplay);

            List<SpanApplier.SpanInfo> spans = new ArrayList<>();
            if (privacyPolicySpan != null) {
                spans.add(privacyPolicySpan);
            }
            if (termsOfServiceSpan != null) {
                spans.add(termsOfServiceSpan);
            }

            SpannableString span =
                    SpanApplier.applySpans(consentText, spans.toArray(new SpanApplier.SpanInfo[0]));
            TextView textView = view.findViewById(R.id.user_data_sharing_consent);
            textView.setText(span);
            textView.setMovementMethod(LinkMovementMethod.getInstance());
        } else {
            assert false : "Unhandled update to property:" + key;
        }
    }

    /**
     * Called whenever IDP sign in is bound to this view.
     * @param model The model containing the data for the view.
     * @param view The view to be bound.
     * @param key The key of the property to be bound.
     */
    static void bindIdpSignInView(PropertyModel model, View view, PropertyKey key) {
        if (key != IdpSignInProperties.IDP_FOR_DISPLAY) {
            assert false : "Unhandled update to property: " + key;
            return;
        }
        String idpForDisplay = model.get(IdpSignInProperties.IDP_FOR_DISPLAY);
        Context context = view.getContext();
        TextView textView = view.findViewById(R.id.idp_signin);
        textView.setText(String.format(
                context.getString(R.string.idp_signin_status_mismatch_dialog_body, idpForDisplay)));
        textView.setMovementMethod(LinkMovementMethod.getInstance());
    }

    /**
     * Called whenever error summary is bound to this view.
     * @param model The model containing the data for the view.
     * @param view The view to be bound.
     * @param key The key of the property to be bound.
     */
    static void bindErrorSummaryView(PropertyModel model, View view, PropertyKey key) {
        if (key != ErrorProperties.IDP_FOR_DISPLAY) {
            assert false : "Unhandled update to property: " + key;
            return;
        }
        String idpForDisplay = model.get(ErrorProperties.IDP_FOR_DISPLAY);
        Context context = view.getContext();
        TextView textView = view.findViewById(R.id.error_summary);
        textView.setText(String.format(
                context.getString(R.string.signin_generic_error_dialog_summary, idpForDisplay)));
        textView.setMovementMethod(LinkMovementMethod.getInstance());
    }

    /**
     * Called whenever error description is bound to this view.
     * @param model The model containing the data for the view.
     * @param view The view to be bound.
     * @param key The key of the property to be bound.
     */
    static void bindErrorDescriptionView(PropertyModel model, View view, PropertyKey key) {
        if (key != ErrorProperties.IDP_FOR_DISPLAY) {
            assert false : "Unhandled update to property: " + key;
            return;
        }
        Context context = view.getContext();
        TextView textView = view.findViewById(R.id.error_description);
        textView.setText(
                String.format(context.getString(R.string.signin_generic_error_dialog_description)));
        textView.setMovementMethod(LinkMovementMethod.getInstance());
    }

    /**
     * Called whenever a continue button for a single account is bound to this view.
     * @param model The model containing the data for the view.
     * @param view The view to be bound.
     * @param key The key of the property to be bound.
     */
    @SuppressWarnings("checkstyle:SetTextColorAndSetTextSizeCheck")
    static void bindContinueButtonView(PropertyModel model, View view, PropertyKey key) {
        Context context = view.getContext();
        ButtonCompat button = view.findViewById(R.id.account_selection_continue_btn);
        if (key == ContinueButtonProperties.IDP_METADATA) {
            if (!ColorUtils.inNightMode(context)) {
                IdentityProviderMetadata idpMetadata =
                        model.get(ContinueButtonProperties.IDP_METADATA);

                Integer backgroundColor = idpMetadata.getBrandBackgroundColor();
                if (backgroundColor != null) {
                    button.setButtonColor(ColorStateList.valueOf(backgroundColor));

                    Integer textColor = idpMetadata.getBrandTextColor();
                    if (textColor == null) {
                        textColor = MaterialColors.getColor(context,
                                ColorUtils.shouldUseLightForegroundOnBackground(backgroundColor)
                                        ? R.attr.colorOnPrimary
                                        : R.attr.colorOnSurface,
                                TAG);
                    }
                    button.setTextColor(textColor);
                }
            }
        } else if (key == ContinueButtonProperties.ACCOUNT) {
            String btnText;
            Account account = model.get(ContinueButtonProperties.ACCOUNT);
            if (account != null) {
                // Prefers to use given name if it is provided otherwise falls back to using the
                // name.
                String givenName = account.getGivenName();
                String displayedName =
                        givenName != null && !givenName.isEmpty() ? givenName : account.getName();
                btnText = String.format(
                        context.getString(R.string.account_selection_continue), displayedName);
            } else {
                btnText = String.format(
                        context.getString(R.string.idp_signin_status_mismatch_dialog_continue));
            }
            assert btnText != null;
            button.setText(btnText);
        } else if (key == ContinueButtonProperties.ON_CLICK_LISTENER) {
            button.setOnClickListener(clickedView -> {
                Account account = model.get(ContinueButtonProperties.ACCOUNT);
                model.get(ContinueButtonProperties.ON_CLICK_LISTENER).onResult(account);
            });
        } else {
            assert false : "Unhandled update to property:" + key;
        }
    }

    /**
     * Called whenever a button on the error dialog is bound to this view.
     * @param model The model containing the data for the view.
     * @param view The view to be bound.
     * @param key The key of the property to be bound.
     * @param button The button to be bound.
     * @param buttonText The text that should be set to the button to be bound.
     */
    @SuppressWarnings("checkstyle:SetTextColorAndSetTextSizeCheck")
    private static void bindErrorButtonView(
            PropertyModel model, View view, PropertyKey key, ButtonCompat button, int textId) {
        Context context = view.getContext();
        if (key == ErrorButtonProperties.IDP_METADATA) {
            String buttonText = String.format(context.getString(textId));
            button.setText(buttonText);
            if (!ColorUtils.inNightMode(context)) {
                IdentityProviderMetadata idpMetadata =
                        model.get(ErrorButtonProperties.IDP_METADATA);

                // TODO(crbug.com/1484245): Decide on how to set colours for error buttons.
                Integer textColor = idpMetadata.getBrandBackgroundColor();
                button.setTextColor(textColor != null
                                ? textColor
                                : MaterialColors.getColor(context, R.attr.colorOnPrimary, TAG));
            }
        } else if (key == ErrorButtonProperties.ON_CLICK_LISTENER) {
            button.setOnClickListener(
                    clickedView -> { model.get(ErrorButtonProperties.ON_CLICK_LISTENER).run(); });
        } else {
            assert false : "Unhandled update to property:" + key;
        }
    }

    /**
     * Called whenever the got it button on the error dialog is bound to this view.
     * @param model The model containing the data for the view.
     * @param view The view to be bound.
     * @param key The key of the property to be bound.
     */
    @SuppressWarnings("checkstyle:SetTextColorAndSetTextSizeCheck")
    static void bindGotItButtonView(PropertyModel model, View view, PropertyKey key) {
        ButtonCompat button = view.findViewById(R.id.got_it_btn);
        bindErrorButtonView(model, view, key, button, R.string.signin_error_dialog_got_it_button);
    }

    /**
     * Called whenever the more details button on the error dialog is bound to this view.
     * @param model The model containing the data for the view.
     * @param view The view to be bound.
     * @param key The key of the property to be bound.
     */
    @SuppressWarnings("checkstyle:SetTextColorAndSetTextSizeCheck")
    static void bindMoreDetailsButtonView(PropertyModel model, View view, PropertyKey key) {
        ButtonCompat button = view.findViewById(R.id.more_details_btn);
        bindErrorButtonView(
                model, view, key, button, R.string.signin_error_dialog_more_details_button);
    }

    /**
     * Called whenever non-account views are bound to the bottom sheet.
     * @param model The model containing the data for the view.
     * @param view The view to be bound.
     * @param key The key of the property to be bound.
     */
    static void bindContentView(PropertyModel model, View view, PropertyKey key) {
        PropertyModel itemModel = model.get((WritableObjectPropertyKey<PropertyModel>) key);
        View itemView = null;
        ViewBinder<PropertyModel, View, PropertyKey> itemBinder = null;
        if (key == ItemProperties.HEADER) {
            itemView = view.findViewById(R.id.header_view_item);
            itemBinder = AccountSelectionViewBinder::bindHeaderView;
        } else if (key == ItemProperties.CONTINUE_BUTTON) {
            itemView = view.findViewById(R.id.account_selection_continue_btn);
            itemBinder = AccountSelectionViewBinder::bindContinueButtonView;
        } else if (key == ItemProperties.DATA_SHARING_CONSENT) {
            itemView = view.findViewById(R.id.user_data_sharing_consent);
            itemBinder = AccountSelectionViewBinder::bindDataSharingConsentView;
        } else if (key == ItemProperties.IDP_SIGNIN) {
            itemView = view.findViewById(R.id.idp_signin);
            itemBinder = AccountSelectionViewBinder::bindIdpSignInView;
        } else if (key == ItemProperties.ERROR_SUMMARY) {
            itemView = view.findViewById(R.id.error_summary);
            itemBinder = AccountSelectionViewBinder::bindErrorSummaryView;
        } else if (key == ItemProperties.ERROR_DESCRIPTION) {
            itemView = view.findViewById(R.id.error_description);
            itemBinder = AccountSelectionViewBinder::bindErrorDescriptionView;
        } else if (key == ItemProperties.GOT_IT_BUTTON) {
            itemView = view.findViewById(R.id.got_it_btn);
            itemBinder = AccountSelectionViewBinder::bindGotItButtonView;
        } else if (key == ItemProperties.MORE_DETAILS_BUTTON) {
            itemView = view.findViewById(R.id.more_details_btn);
            itemBinder = AccountSelectionViewBinder::bindMoreDetailsButtonView;
        } else {
            assert false : "Unhandled update to property:" + key;
            return;
        }

        if (itemModel == null) {
            itemView.setVisibility(View.GONE);
            return;
        }

        itemView.setVisibility(View.VISIBLE);
        for (PropertyKey itemKey : itemModel.getAllSetProperties()) {
            itemBinder.bind(itemModel, itemView, itemKey);
        }
    }

    /**
     * Called whenever a header is bound to this view.
     * @param model The model containing the data for the view.
     * @param view The view to be bound.
     * @param key The key of the property to be bound.
     */
    static void bindHeaderView(PropertyModel model, View view, PropertyKey key) {
        if (key == HeaderProperties.TOP_FRAME_FOR_DISPLAY
                || key == HeaderProperties.IFRAME_FOR_DISPLAY
                || key == HeaderProperties.IDP_FOR_DISPLAY || key == HeaderProperties.TYPE
                || key == HeaderProperties.RP_CONTEXT) {
            Resources resources = view.getResources();
            TextView headerTitleText = view.findViewById(R.id.header_title);
            TextView headerSubtitleText = view.findViewById(R.id.header_subtitle);
            HeaderProperties.HeaderType headerType = model.get(HeaderProperties.TYPE);

            String rpUrlForDisplayInTitle;
            String subtitle = computeHeaderSubtitle(resources, headerType,
                    model.get(HeaderProperties.TOP_FRAME_FOR_DISPLAY),
                    model.get(HeaderProperties.IFRAME_FOR_DISPLAY));
            if (!subtitle.isEmpty()) {
                headerTitleText.setPadding(/*left=*/0, /*top=*/12, /*right=*/0, /*bottom=*/0);
                headerSubtitleText.setText(subtitle);
                rpUrlForDisplayInTitle = model.get(HeaderProperties.IFRAME_FOR_DISPLAY);
            } else {
                headerSubtitleText.setVisibility(View.GONE);
                rpUrlForDisplayInTitle = model.get(HeaderProperties.TOP_FRAME_FOR_DISPLAY);
            }

            String title = computeHeaderTitle(resources, headerType, rpUrlForDisplayInTitle,
                    model.get(HeaderProperties.IDP_FOR_DISPLAY),
                    model.get(HeaderProperties.RP_CONTEXT));
            headerTitleText.setText(title);
            headerTitleText.setMovementMethod(LinkMovementMethod.getInstance());

            // Make instructions for closing the bottom sheet part of the header's content
            // description. This is needed because the bottom sheet's content description (which
            // includes instructions to close the bottom sheet) is not announced when the FedCM
            // bottom sheet is shown. Don't include instructions for closing the bottom sheet as
            // part of the "Verifying..." header content description because the bottom sheet
            // closes itself automatically at the "Verifying..." stage.
            if (headerType != HeaderProperties.HeaderType.VERIFY
                    && headerType != HeaderProperties.HeaderType.VERIFY_AUTO_REAUTHN) {
                headerTitleText.setContentDescription(title + ". "
                        + resources.getString(R.string.bottom_sheet_accessibility_description));
            } else {
                // Update the content description in case the view is recycled.
                headerTitleText.setContentDescription(title);
            }

            if (key == HeaderProperties.TYPE) {
                boolean progressBarVisible = (headerType == HeaderProperties.HeaderType.VERIFY
                        || headerType == HeaderProperties.HeaderType.VERIFY_AUTO_REAUTHN);
                view.findViewById(R.id.header_progress_bar)
                        .setVisibility(progressBarVisible ? View.VISIBLE : View.GONE);
                view.findViewById(R.id.header_divider)
                        .setVisibility(!progressBarVisible ? View.VISIBLE : View.GONE);
            }
        } else if (key == HeaderProperties.IDP_BRAND_ICON) {
            Bitmap brandIcon = model.get(HeaderProperties.IDP_BRAND_ICON);
            if (brandIcon != null) {
                Resources resources = view.getResources();
                int iconSize =
                        resources.getDimensionPixelSize(R.dimen.account_selection_sheet_icon_size);
                Drawable croppedBrandIcon =
                        createBitmapWithMaskableIconSafeZone(resources, brandIcon, iconSize);
                ImageView headerIconView = (ImageView) view.findViewById(R.id.header_idp_icon);
                headerIconView.setImageDrawable(croppedBrandIcon);
                headerIconView.setVisibility(View.VISIBLE);
            }
        } else if (key == HeaderProperties.CLOSE_ON_CLICK_LISTENER) {
            final Runnable closeOnClickRunnable =
                    (Runnable) model.get(HeaderProperties.CLOSE_ON_CLICK_LISTENER);
            view.findViewById(R.id.close_button).setOnClickListener(clickedView -> {
                closeOnClickRunnable.run();
            });
        } else {
            assert false : "Unhandled update to property:" + key;
        }
    }

    /**
     * Returns text for the {@link HeaderType.VERIFY} header.
     */
    static @StringRes int getVerifyHeaderStringId() {
        return R.string.verify_sheet_title;
    }

    /**
     * Returns text for the {@link HeaderType.VERIFY_AUTO_REAUTHN} header.
     */
    static @StringRes int getVerifyHeaderAutoReauthnStringId() {
        return R.string.verify_sheet_title_auto_reauthn;
    }

    private static String computeHeaderTitle(Resources resources, HeaderProperties.HeaderType type,
            String rpUrl, String idpUrl, String rpContext) {
        if (type == HeaderProperties.HeaderType.VERIFY) {
            return resources.getString(getVerifyHeaderStringId());
        }
        if (type == HeaderProperties.HeaderType.VERIFY_AUTO_REAUTHN) {
            return resources.getString(getVerifyHeaderAutoReauthnStringId());
        }
        @StringRes
        int titleStringId;
        switch (rpContext) {
            case "signup":
                titleStringId = R.string.account_selection_sheet_title_explicit_signup;
                break;
            case "use":
                titleStringId = R.string.account_selection_sheet_title_explicit_use;
                break;
            case "continue":
                titleStringId = R.string.account_selection_sheet_title_explicit_continue;
                break;
            default:
                titleStringId = R.string.account_selection_sheet_title_explicit_signin;
        }
        return String.format(resources.getString(titleStringId), rpUrl, idpUrl);
    }

    private static String computeHeaderSubtitle(Resources resources,
            HeaderProperties.HeaderType type, String topFrameUrl, String iframeUrl) {
        if (type == HeaderProperties.HeaderType.VERIFY
                || type == HeaderProperties.HeaderType.VERIFY_AUTO_REAUTHN || iframeUrl.isEmpty()) {
            return "";
        }
        @StringRes
        int subtitleStringId = R.string.account_selection_sheet_subtitle_explicit;
        return String.format(resources.getString(subtitleStringId), topFrameUrl);
    }

    private AccountSelectionViewBinder() {}
}
