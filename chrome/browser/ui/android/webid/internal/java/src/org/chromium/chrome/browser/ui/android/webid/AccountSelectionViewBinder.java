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
import android.icu.text.ListFormatter;
import android.text.SpannableString;
import android.text.method.LinkMovementMethod;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ImageView;
import android.widget.TextView;

import androidx.annotation.StringRes;

import com.google.android.material.color.MaterialColors;

import org.chromium.base.Callback;
import org.chromium.blink.mojom.RpContext;
import org.chromium.blink.mojom.RpMode;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ui.android.webid.AccountSelectionProperties.AccountProperties;
import org.chromium.chrome.browser.ui.android.webid.AccountSelectionProperties.ButtonData;
import org.chromium.chrome.browser.ui.android.webid.AccountSelectionProperties.ContinueButtonProperties;
import org.chromium.chrome.browser.ui.android.webid.AccountSelectionProperties.DataSharingConsentProperties;
import org.chromium.chrome.browser.ui.android.webid.AccountSelectionProperties.ErrorProperties;
import org.chromium.chrome.browser.ui.android.webid.AccountSelectionProperties.HeaderProperties;
import org.chromium.chrome.browser.ui.android.webid.AccountSelectionProperties.IdpSignInProperties;
import org.chromium.chrome.browser.ui.android.webid.AccountSelectionProperties.ItemProperties;
import org.chromium.chrome.browser.ui.android.webid.AccountSelectionProperties.LoginButtonProperties;
import org.chromium.chrome.browser.ui.android.webid.data.Account;
import org.chromium.chrome.browser.ui.android.webid.data.IdentityProviderData;
import org.chromium.chrome.browser.ui.android.webid.data.IdentityProviderMetadata;
import org.chromium.components.browser_ui.util.AvatarGenerator;
import org.chromium.components.browser_ui.widget.RoundedIconGenerator;
import org.chromium.components.browser_ui.widget.TintedDrawable;
import org.chromium.content.webid.IdentityRequestDialogDisclosureField;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor.ViewBinder;
import org.chromium.ui.text.ChromeClickableSpan;
import org.chromium.ui.text.SpanApplier;
import org.chromium.ui.util.ColorUtils;
import org.chromium.ui.widget.ButtonCompat;
import org.chromium.url.GURL;

import java.util.ArrayList;
import java.util.List;
import java.util.Locale;
import java.util.function.Consumer;

/**
 * Provides functions that map {@link AccountSelectionProperties} changes in a {@link PropertyModel}
 * to the suitable method in {@link AccountSelectionView}.
 */
class AccountSelectionViewBinder {
    private static final String TAG = "AccountSelectionView";

    // Error codes used to show more specific error UI to the user.
    static final String GENERIC = "";
    static final String INVALID_REQUEST = "invalid_request";
    static final String UNAUTHORIZED_CLIENT = "unauthorized_client";
    static final String ACCESS_DENIED = "access_denied";
    static final String TEMPORARILY_UNAVAILABLE = "temporarily_unavailable";
    static final String SERVER_ERROR = "server_error";

    static final float DISABLED_OPACITY = 0.38f;

    /**
     * Returns bitmap with the maskable bitmap's safe zone as defined in
     * https://www.w3.org/TR/appmanifest/ cropped in a circle.
     *
     * @param resources the Resources used to set initial target density.
     * @param bitmap the maskable bitmap. It should adhere to the maskable icon spec as defined in
     *     https://www.w3.org/TR/appmanifest/
     * @param outBitmapSize the target bitmap size in pixels.
     * @return the cropped bitmap.
     */
    public static Drawable createBitmapWithMaskableIconSafeZone(
            Resources resources, Bitmap bitmap, int outBitmapSize) {
        int cropWidth =
                (int)
                        Math.floor(
                                bitmap.getWidth()
                                        * AccountSelectionBridge
                                                .MASKABLE_ICON_SAFE_ZONE_DIAMETER_RATIO);
        int cropHeight =
                (int)
                        Math.floor(
                                bitmap.getHeight()
                                        * AccountSelectionBridge
                                                .MASKABLE_ICON_SAFE_ZONE_DIAMETER_RATIO);
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
        canvas.drawBitmap(
                bitmap,
                new Rect(cropX, cropY, cropWidth + cropX, cropHeight + cropY),
                new Rect(0, 0, outBitmapSize, outBitmapSize),
                paint);
        return new BitmapDrawable(resources, output);
    }

    static void updateAccountViewAvatar(PropertyModel model, View view) {
        AccountProperties.Avatar avatarData = model.get(AccountProperties.AVATAR);
        if (avatarData == null) return;
        int avatarSize = avatarData.mAvatarSize;
        Bitmap avatar = avatarData.mAvatar;
        ImageView avatarView = view.findViewById(R.id.start_icon);
        Resources resources = view.getContext().getResources();
        if (model.get(AccountProperties.SHOW_IDP)) {
            // Resize the image view and the margin to account for the badging.
            ViewGroup.LayoutParams layoutParams = avatarView.getLayoutParams();
            ViewGroup.MarginLayoutParams marginLayoutParams =
                    (ViewGroup.MarginLayoutParams) layoutParams;
            int size =
                    resources.getDimensionPixelSize(
                            R.dimen.account_selection_account_avatar_multi_idp_size);
            int marginEnd =
                    resources.getDimensionPixelSize(
                            R.dimen.account_selection_account_avatar_multi_idp_margin_end);
            layoutParams.width = size;
            layoutParams.height = size;
            marginLayoutParams.setMarginEnd(marginEnd);

            // In this case, we expect the image to be badged and cropped, so we set the image
            // directly instead of using the monogram and invoking AvatarGenerator.makeRoundAvatar.
            Bitmap output = Bitmap.createBitmap(avatarSize, avatarSize, Config.ARGB_8888);
            Canvas canvas = new Canvas(output);
            Paint paint = new Paint();
            paint.setAntiAlias(true);
            canvas.drawBitmap(avatar, null, new Rect(0, 0, avatarSize, avatarSize), paint);
            avatarView.setImageDrawable(new BitmapDrawable(resources, output));
            return;
        }

        // Prepare avatar or its fallback monogram.
        if (avatar == null) {
            int avatarMonogramTextSize =
                    view.getResources()
                            .getDimensionPixelSize(
                                    R.dimen.account_selection_account_avatar_monogram_text_size);
            RoundedIconGenerator roundedIconGenerator =
                    new RoundedIconGenerator(
                            resources,
                            /* iconWidthDp= */ avatarSize,
                            /* iconHeightDp= */ avatarSize,
                            /* cornerRadiusDp= */ avatarSize / 2,
                            /* backgroundColor= */ Color.GRAY,
                            avatarMonogramTextSize);
            avatar = roundedIconGenerator.generateIconForText(avatarData.mDisplayName);
        }
        Drawable croppedAvatar = AvatarGenerator.makeRoundAvatar(resources, avatar, avatarSize);
        avatarView.setImageDrawable(croppedAvatar);
    }

    /**
     * Called whenever an account is bound to this view.
     *
     * @param model The model containing the data for the view.
     * @param view The view to be bound.
     * @param key The key of the property to be bound.
     */
    static void bindAccountView(PropertyModel model, View view, PropertyKey key) {
        Account account = model.get(AccountProperties.ACCOUNT);
        if (key == AccountProperties.ON_CLICK_LISTENER) {
            Callback<ButtonData> clickCallback = model.get(AccountProperties.ON_CLICK_LISTENER);
            if (clickCallback == null) {
                view.setOnClickListener(null);
            } else {
                view.setOnClickListener(
                        clickedView -> {
                            clickCallback.onResult(
                                    new ButtonData(account, /* idpMetadata= */ null));
                        });
            }
            return;
        }
        if (key == AccountProperties.AVATAR) {
            updateAccountViewAvatar(model, view);
        } else if (key == AccountProperties.ACCOUNT) {
            if (account.isFilteredOut()) {
                view.setAlpha(DISABLED_OPACITY);
            }
            TextView title = view.findViewById(R.id.title);
            // Name is not shown in the account chip of the request permission dialog. The name is
            // shown in the Continue button instead.
            if (title != null) {
                title.setText(
                        account.isFilteredOut() && !account.getDisplayIdentifier().isEmpty()
                                ? account.getDisplayIdentifier()
                                : account.getDisplayName());
            }
            TextView description = view.findViewById(R.id.description);
            String descriptionText =
                    account.isFilteredOut()
                            ? view.getContext().getString(R.string.filtered_account_message)
                            : account.getDisplayIdentifier();
            if (descriptionText.isEmpty() && title == null) {
                // It is possible that the display identifier is empty.
                // If we have no title, show the display name in the description.
                descriptionText = account.getDisplayName();
            }
            if (descriptionText.isEmpty()) {
                // Hide the view so that we center the remaining view(s).
                description.setVisibility(View.GONE);
            } else {
                description.setText(descriptionText);
                description.setVisibility(View.VISIBLE);
            }
            TextView secondaryDescription = view.findViewById(R.id.secondary_description);
            // The secondary description is not shown in the account chip of active mode's
            // request permission dialog. In this case, the view is not present.
            if (secondaryDescription == null) {
                return;
            }
            if (model.get(AccountProperties.SHOW_IDP)
                    && account.getSecondaryDescription() != null) {
                if (account.isSignIn()) {
                    // Include a hint that this is a returning account.
                    secondaryDescription.setText(
                            view.getContext()
                                    .getString(
                                            R.string.account_selection_returning_account_message,
                                            account.getSecondaryDescription()));
                } else {
                    secondaryDescription.setText(account.getSecondaryDescription());
                }
                secondaryDescription.setVisibility(View.VISIBLE);
            } else {
                secondaryDescription.setVisibility(View.GONE);
            }
        } else if (key != AccountProperties.SHOW_IDP) {
            assert false : "Unhandled update to property:" + key;
        }
    }

    /**
     * Called whenever a login button is bound to this view.
     *
     * @param model The model containing the data for the view.
     * @param view The view to be bound.
     * @param key The key of the property to be bound.
     */
    @SuppressWarnings("checkstyle:SetTextColorAndSetTextSizeCheck")
    static void bindLoginButtonView(PropertyModel model, View view, PropertyKey key) {
        if (key == LoginButtonProperties.PROPERTIES) {
            LoginButtonProperties.Properties properties =
                    model.get(LoginButtonProperties.PROPERTIES);
            Context context = view.getContext();

            // If startIconView is available, the add account button is an account row at the end of
            // the accounts list.
            ImageView startIconView = view.findViewById(R.id.start_icon);
            IdentityProviderData identityProvider = properties.mIdentityProvider;
            IdentityProviderMetadata idpMetadata = identityProvider.getIdpMetadata();
            Boolean showIdp = properties.mShowIdp;
            if (startIconView != null) {
                Bitmap brandIcon = idpMetadata.getBrandIconBitmap();
                ImageView endIconView = view.findViewById(R.id.end_icon);
                if (!showIdp || brandIcon == null) {
                    setDefaultLoginImage(context, properties, startIconView);
                    if (endIconView != null) {
                        endIconView.setVisibility(View.GONE);
                    }
                } else {
                    Resources resources = view.getResources();
                    int iconSize =
                            resources.getDimensionPixelSize(
                                    R.dimen.account_selection_login_brand_icon_size);
                    Drawable croppedBrandIcon =
                            createBitmapWithMaskableIconSafeZone(resources, brandIcon, iconSize);
                    startIconView.setImageDrawable(croppedBrandIcon);

                    if (endIconView != null) {
                        setDefaultLoginImage(context, properties, endIconView);
                        endIconView.setVisibility(View.VISIBLE);
                    }
                }

                TextView subject = view.findViewById(R.id.title);
                String buttonText =
                        showIdp
                                ? context.getString(
                                        R.string.account_selection_add_account_with_origin,
                                        identityProvider.getIdpForDisplay())
                                : context.getString(R.string.account_selection_add_account);
                subject.setText(buttonText);
                String buttonTextWithOpensInNewTab =
                        context.getString(
                                R.string.account_selection_add_account_opens_in_new_tab,
                                buttonText);
                subject.setContentDescription(buttonTextWithOpensInNewTab);

                view.setOnClickListener(
                        clickedView -> {
                            properties.mOnClickListener.onResult(
                                    new ButtonData(/* account= */ null, idpMetadata));
                        });
                return;
            }

            // Since startIconView is not available, the add account button is a secondary button
            // under the continue button at the bottom of the screen.
            ButtonCompat button = view.findViewById(R.id.account_selection_add_account_btn);
            button.setOnClickListener(
                    clickedView -> {
                        properties.mOnClickListener.onResult(
                                new ButtonData(/* account= */ null, idpMetadata));
                    });
            button.setText(context.getString(R.string.account_selection_add_account));

            if (!ColorUtils.inNightMode(context)) {
                Integer backgroundColor = idpMetadata.getBrandBackgroundColor();
                if (backgroundColor != null) {
                    // Set background color as text color because this is a secondary button which
                    // has inverted colors compared to the primary button.
                    button.setTextColor(backgroundColor);
                }
            }
        } else {
            assert false : "Unhandled update to property:" + key;
        }
    }

    static void setDefaultLoginImage(
            Context context, LoginButtonProperties.Properties properties, ImageView iconView) {
        TintedDrawable plusIcon =
                TintedDrawable.constructTintedDrawable(
                        context,
                        properties.mRpMode == RpMode.ACTIVE
                                ? R.drawable.plus
                                : R.drawable.open_in_new_tab,
                        properties.mRpMode == RpMode.ACTIVE
                                ? R.color.default_icon_color_accent1_tint_list
                                : R.color.default_icon_color_tint_list);
        iconView.setImageDrawable(plusIcon);
    }

    static SpanApplier.SpanInfo createLink(
            Context context, String tag, GURL url, Consumer<Context> clickCallback) {
        if (GURL.isEmptyOrInvalid(url)) return null;

        String startTag = "<" + tag + ">";
        String endTag = "</" + tag + ">";
        Callback<View> onClickCallback =
                v -> {
                    clickCallback.accept(context);
                };
        return new SpanApplier.SpanInfo(
                startTag, endTag, new ChromeClickableSpan(context, onClickCallback));
    }

    /**
     * Called whenever a user data sharing consent is bound to this view.
     *
     * @param model The model containing the data for the view.
     * @param view The view to be bound.
     * @param key The key of the property to be bound.
     */
    static void bindDataSharingConsentView(PropertyModel model, View view, PropertyKey key) {
        if (key == DataSharingConsentProperties.PROPERTIES) {
            DataSharingConsentProperties.Properties properties =
                    model.get(DataSharingConsentProperties.PROPERTIES);

            Context context = view.getContext();
            ArrayList<String> fieldStrings = new ArrayList<String>();
            for (@IdentityRequestDialogDisclosureField int field : properties.mDisclosureFields) {
                switch (field) {
                    case IdentityRequestDialogDisclosureField.NAME:
                        fieldStrings.add(
                                context.getString(R.string.account_selection_data_sharing_name));
                        break;
                    case IdentityRequestDialogDisclosureField.EMAIL:
                        fieldStrings.add(
                                context.getString(R.string.account_selection_data_sharing_email));
                        break;
                    case IdentityRequestDialogDisclosureField.PICTURE:
                        fieldStrings.add(
                                context.getString(R.string.account_selection_data_sharing_picture));
                        break;
                    case IdentityRequestDialogDisclosureField.PHONE_NUMBER:
                        fieldStrings.add(
                                context.getString(R.string.account_selection_data_sharing_phone));
                        break;
                    case IdentityRequestDialogDisclosureField.USERNAME:
                        fieldStrings.add(
                                context.getString(
                                        R.string.account_selection_data_sharing_username));
                        break;
                }
            }
            ListFormatter formatter = ListFormatter.getInstance(Locale.getDefault());
            String allFields = formatter.format(fieldStrings);

            SpanApplier.SpanInfo privacyPolicySpan =
                    createLink(
                            context,
                            "link_privacy_policy",
                            properties.mPrivacyPolicyUrl,
                            properties.mPrivacyPolicyClickCallback);
            SpanApplier.SpanInfo termsOfServiceSpan =
                    createLink(
                            context,
                            "link_terms_of_service",
                            properties.mTermsOfServiceUrl,
                            properties.mTermsOfServiceClickCallback);

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
                    context.getString(consentTextId, properties.mIdpForDisplay, allFields);

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
            if (properties.mSetFocusViewCallback != null) {
                properties.mSetFocusViewCallback.onResult(textView);
            }
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
        textView.setText(
                context.getString(R.string.idp_signin_status_mismatch_dialog_body, idpForDisplay));
        textView.setMovementMethod(LinkMovementMethod.getInstance());
    }

    static class ErrorText {
        final String mSummary;
        final SpannableString mDescription;

        ErrorText(String summary, String description) {
            mSummary = summary;
            mDescription = new SpannableString(description);
        }

        ErrorText(String summary, String description, Context context, Runnable runnable) {
            mSummary = summary;
            SpanApplier.SpanInfo moreDetailsSpan =
                    new SpanApplier.SpanInfo(
                            "<link_more_details>",
                            "</link_more_details>",
                            new ChromeClickableSpan(context, (View clickedView) -> runnable.run()));
            mDescription = SpanApplier.applySpans(description, moreDetailsSpan);
        }
    }

    /**
     * Returns text to be displayed on the error dialog.
     *
     * @param context The context of the view to be bound.
     * @param properties The properties which determine what error text to display.
     * @param clickableText Whether the text should contain a link for more details.
     * @return The ErrorText containing the summary and description to display.
     */
    static ErrorText getErrorText(
            Context context, ErrorProperties.Properties properties, boolean clickableText) {
        String code = properties.mError.getCode();
        GURL url = properties.mError.getUrl();
        String idpForDisplay = properties.mIdpForDisplay;
        String rpForDisplay = properties.mRpForDisplay;

        String summary;
        String description;

        if (SERVER_ERROR.equals(code)) {
            summary = context.getString(R.string.signin_server_error_dialog_summary);
            description =
                    context.getString(
                            R.string.signin_server_error_dialog_description, rpForDisplay);
            // Server errors do not need extra description to be displayed.
            return new ErrorText(summary, description);
        } else if (INVALID_REQUEST.equals(code)) {
            summary =
                    context.getString(
                            R.string.signin_invalid_request_error_dialog_summary,
                            rpForDisplay,
                            idpForDisplay);
            description =
                    context.getString(R.string.signin_invalid_request_error_dialog_description);
        } else if (UNAUTHORIZED_CLIENT.equals(code)) {
            summary =
                    context.getString(
                            R.string.signin_unauthorized_client_error_dialog_summary,
                            rpForDisplay,
                            idpForDisplay);
            description =
                    context.getString(R.string.signin_unauthorized_client_error_dialog_description);
        } else if (ACCESS_DENIED.equals(code)) {
            summary = context.getString(R.string.signin_access_denied_error_dialog_summary);
            description = context.getString(R.string.signin_access_denied_error_dialog_description);
        } else if (TEMPORARILY_UNAVAILABLE.equals(code)) {
            summary =
                    context.getString(R.string.signin_temporarily_unavailable_error_dialog_summary);
            description =
                    context.getString(
                            R.string.signin_temporarily_unavailable_error_dialog_description,
                            idpForDisplay);
        } else {
            summary =
                    context.getString(R.string.signin_generic_error_dialog_summary, idpForDisplay);
            description = context.getString(R.string.signin_generic_error_dialog_description);

            if (url.isEmpty() || !clickableText) {
                return new ErrorText(summary, description);
            }

            description += ". ";
            description +=
                    context.getString(R.string.signin_generic_error_dialog_more_details_prompt);
            return new ErrorText(
                    summary, description, context, properties.mMoreDetailsClickRunnable);
        }

        if (url.isEmpty()) {
            description += " ";
            description +=
                    context.getString(
                            TEMPORARILY_UNAVAILABLE.equals(code)
                                    ? R.string.signin_error_dialog_try_other_ways_retry_prompt
                                    : R.string.signin_error_dialog_try_other_ways_prompt,
                            rpForDisplay);
            return new ErrorText(summary, description);
        }

        description += " ";
        description +=
                context.getString(
                        TEMPORARILY_UNAVAILABLE.equals(code)
                                ? (clickableText
                                        ? R.string.signin_error_dialog_more_details_retry_prompt
                                        : R.string
                                                .signin_error_dialog_more_details_button_retry_prompt)
                                : (clickableText
                                        ? R.string.signin_error_dialog_more_details_prompt
                                        : R.string.signin_error_dialog_more_details_button_prompt),
                        idpForDisplay);

        if (clickableText) {
            return new ErrorText(
                    summary, description, context, properties.mMoreDetailsClickRunnable);
        }
        return new ErrorText(summary, description);
    }

    /**
     * Called whenever error text is bound to this view.
     *
     * @param model The model containing the data for the view.
     * @param view The view to be bound.
     * @param key The key of the property to be bound.
     */
    static void bindErrorTextView(PropertyModel model, View view, PropertyKey key) {
        if (key == ErrorProperties.PROPERTIES) {
            ErrorText errorText =
                    getErrorText(
                            view.getContext(),
                            model.get(ErrorProperties.PROPERTIES),
                            /* clickableText= */ true);

            TextView summaryTextView = view.findViewById(R.id.error_summary);
            summaryTextView.setText(errorText.mSummary);
            summaryTextView.setMovementMethod(LinkMovementMethod.getInstance());

            TextView descriptionTextView = view.findViewById(R.id.error_description);
            descriptionTextView.setText(errorText.mDescription);
            descriptionTextView.setMovementMethod(LinkMovementMethod.getInstance());
        } else {
            assert false : "Unhandled update to property:" + key;
        }
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

        if (key == ContinueButtonProperties.PROPERTIES) {
            ContinueButtonProperties.Properties properties =
                    model.get(ContinueButtonProperties.PROPERTIES);

            IdentityProviderMetadata idpMetadata = properties.mIdpMetadata;
            if (!ColorUtils.inNightMode(context)) {
                Integer backgroundColor = idpMetadata.getBrandBackgroundColor();
                if (backgroundColor != null) {
                    button.setButtonColor(ColorStateList.valueOf(backgroundColor));

                    Integer textColor = idpMetadata.getBrandTextColor();
                    if (textColor == null) {
                        textColor =
                                MaterialColors.getColor(
                                        context,
                                        ColorUtils.shouldUseLightForegroundOnBackground(
                                                        backgroundColor)
                                                ? R.attr.colorOnPrimary
                                                : R.attr.colorOnSurface,
                                        TAG);
                    }
                    button.setTextColor(textColor);
                }
            }

            Account account = properties.mAccount;
            button.setOnClickListener(
                    clickedView -> {
                        properties.mOnClickListener.onResult(
                                new ButtonData(account, properties.mIdpMetadata));
                    });

            String btnText;
            HeaderProperties.HeaderType headerType = properties.mHeaderType;
            if (headerType == HeaderProperties.HeaderType.SIGN_IN_TO_IDP_STATIC) {
                btnText = context.getString(R.string.signin_continue);
                button.setContentDescription(
                        context.getString(
                                R.string.account_selection_add_account_opens_in_new_tab, btnText));
            } else if (headerType == HeaderProperties.HeaderType.SIGN_IN_ERROR) {
                btnText = context.getString(R.string.signin_error_dialog_got_it_button);
            } else {
                String givenName = account.getGivenName();
                if (givenName.isEmpty()) {
                    btnText = context.getString(R.string.signin_continue);
                } else {
                    btnText =
                            String.format(
                                    context.getString(R.string.account_selection_continue),
                                    givenName);
                }
                button.setContentDescription(btnText + ", " + account.getDisplayIdentifier());
            }

            assert btnText != null;
            button.setText(btnText);
            if (properties.mSetFocusViewCallback != null) {
                properties.mSetFocusViewCallback.onResult(button);
            }
        } else {
            assert false : "Unhandled update to property:" + key;
        }
    }

    /**
     * Called whenever non-account views are bound to the bottom sheet.
     *
     * @param model The model containing the data for the view.
     * @param view The view to be bound.
     * @param key The key of the property to be bound.
     */
    static void bindContentView(PropertyModel model, View view, PropertyKey key) {
        View itemView = null;
        if (key == ItemProperties.SPINNER_ENABLED) {
            itemView = view.findViewById(R.id.spinner);
            if (itemView == null) return;
            itemView.setVisibility(
                    model.get(ItemProperties.SPINNER_ENABLED) ? View.VISIBLE : View.GONE);
            return;
        }
        if (key == ItemProperties.DRAGBAR_HANDLE_VISIBLE) {
            itemView = view.findViewById(R.id.drag_handlebar);
            itemView.setVisibility(
                    model.get(ItemProperties.DRAGBAR_HANDLE_VISIBLE) ? View.VISIBLE : View.GONE);
            return;
        }
        PropertyModel itemModel = model.get((WritableObjectPropertyKey<PropertyModel>) key);
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
        } else if (key == ItemProperties.ERROR_TEXT) {
            itemView = view.findViewById(R.id.error_text);
            itemBinder = AccountSelectionViewBinder::bindErrorTextView;
        } else if (key == ItemProperties.ADD_ACCOUNT_BUTTON) {
            itemView = view.findViewById(R.id.account_selection_add_account_btn);
            itemBinder = AccountSelectionViewBinder::bindLoginButtonView;
        } else if (key == ItemProperties.ACCOUNT_CHIP) {
            itemView = view.findViewById(R.id.account_chip);
            itemBinder = AccountSelectionViewBinder::bindAccountView;
        } else {
            assert false : "Unhandled update to property:" + key;
            return;
        }

        if (itemView == null) {
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
     *
     * @param model The model containing the data for the view.
     * @param view The view to be bound.
     * @param key The key of the property to be bound.
     */
    static void bindHeaderView(PropertyModel model, View view, PropertyKey key) {
        Resources resources = view.getResources();
        View headerView = view.findViewById(R.id.header);

        // Reuse the same header from previous dialog if active mode verify sheet.
        if (model.get(HeaderProperties.RP_MODE) == RpMode.ACTIVE
                && (model.get(HeaderProperties.TYPE) == HeaderProperties.HeaderType.VERIFY
                        || model.get(HeaderProperties.TYPE)
                                == HeaderProperties.HeaderType.VERIFY_AUTO_REAUTHN)) {
            headerView.setContentDescription(resources.getString(R.string.verify_sheet_title));
            if (model.get(HeaderProperties.SET_FOCUS_VIEW_CALLBACK) != null) {
                model.get(HeaderProperties.SET_FOCUS_VIEW_CALLBACK).onResult(headerView);
            }
            return;
        }

        if (key == HeaderProperties.RP_FOR_DISPLAY
                || key == HeaderProperties.IDP_FOR_DISPLAY
                || key == HeaderProperties.TYPE
                || key == HeaderProperties.RP_CONTEXT
                || key == HeaderProperties.RP_MODE
                || key == HeaderProperties.IS_MULTIPLE_ACCOUNT_CHOOSER
                || key == HeaderProperties.SET_FOCUS_VIEW_CALLBACK
                || key == HeaderProperties.IS_MULTIPLE_IDPS) {
            TextView headerTitleText = view.findViewById(R.id.header_title);
            TextView headerSubtitleText = view.findViewById(R.id.header_subtitle);
            HeaderProperties.HeaderType headerType = model.get(HeaderProperties.TYPE);

            String subtitle =
                    computeHeaderSubtitle(
                            resources,
                            model.get(HeaderProperties.RP_FOR_DISPLAY),
                            model.get(HeaderProperties.RP_MODE),
                            model.get(HeaderProperties.IS_MULTIPLE_ACCOUNT_CHOOSER),
                            model.get(HeaderProperties.IS_MULTIPLE_IDPS));
            if (!subtitle.isEmpty()) {
                headerTitleText.setPadding(
                        /* left= */ 0, /* top= */ 12, /* right= */ 0, /* bottom= */ 0);
                if (headerSubtitleText.getText() != subtitle
                        && model.get(HeaderProperties.SET_FOCUS_VIEW_CALLBACK) != null) {
                    model.get(HeaderProperties.SET_FOCUS_VIEW_CALLBACK).onResult(headerView);
                }
                headerSubtitleText.setText(subtitle);
                headerSubtitleText.setMovementMethod(LinkMovementMethod.getInstance());
            } else {
                headerSubtitleText.setVisibility(View.GONE);
            }

            String title =
                    computeHeaderTitle(
                            resources,
                            headerType,
                            model.get(HeaderProperties.RP_FOR_DISPLAY),
                            model.get(HeaderProperties.IDP_FOR_DISPLAY),
                            model.get(HeaderProperties.RP_CONTEXT),
                            model.get(HeaderProperties.RP_MODE),
                            model.get(HeaderProperties.IS_MULTIPLE_IDPS));
            if (headerTitleText.getText() != title
                    && model.get(HeaderProperties.SET_FOCUS_VIEW_CALLBACK) != null) {
                model.get(HeaderProperties.SET_FOCUS_VIEW_CALLBACK).onResult(headerView);
            }
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
                headerView.setContentDescription(
                        title
                                + ". "
                                + subtitle
                                + ". "
                                + resources.getString(
                                        R.string.bottom_sheet_accessibility_description));
            } else {
                // Update the content description in case the view is recycled.
                headerView.setContentDescription(title);
            }

            if (key == HeaderProperties.TYPE) {
                // There is no progress bar or divider in the header for active mode.
                if (model.get(HeaderProperties.RP_MODE) == RpMode.ACTIVE) return;

                boolean progressBarVisible =
                        (headerType == HeaderProperties.HeaderType.VERIFY
                                || headerType == HeaderProperties.HeaderType.VERIFY_AUTO_REAUTHN);
                view.findViewById(R.id.header_progress_bar)
                        .setVisibility(progressBarVisible ? View.VISIBLE : View.GONE);
                view.findViewById(R.id.header_divider)
                        .setVisibility(!progressBarVisible ? View.VISIBLE : View.GONE);
            }
        } else if (key == HeaderProperties.HEADER_ICON) {
            Bitmap brandIcon = model.get(HeaderProperties.HEADER_ICON);
            // Do not crop the header icon when it is the RP icon, e.g. when there are multiple IDPs
            // in the current dialog.
            boolean shouldCircleCrop = !model.get(HeaderProperties.IS_MULTIPLE_IDPS);
            if (brandIcon == null) {
                return;
            }
            int iconSize =
                    resources.getDimensionPixelSize(
                            model.get(HeaderProperties.RP_MODE) == RpMode.ACTIVE
                                    ? R.dimen.account_selection_active_mode_sheet_icon_size
                                    : R.dimen.account_selection_sheet_icon_size);
            ImageView headerIconView = view.findViewById(R.id.header_icon);
            if (shouldCircleCrop) {
                Drawable croppedBrandIcon =
                        createBitmapWithMaskableIconSafeZone(resources, brandIcon, iconSize);
                headerIconView.setImageDrawable(croppedBrandIcon);
            } else {
                Bitmap output = Bitmap.createBitmap(iconSize, iconSize, Config.ARGB_8888);
                Canvas canvas = new Canvas(output);
                Paint paint = new Paint();
                paint.setAntiAlias(true);
                canvas.drawBitmap(brandIcon, null, new Rect(0, 0, iconSize, iconSize), paint);
                headerIconView.setImageDrawable(new BitmapDrawable(resources, output));
            }
            headerIconView.setVisibility(View.VISIBLE);
        } else if (key == HeaderProperties.RP_BRAND_ICON) {
            // RP icon is not shown in passive mode.
            if (model.get(HeaderProperties.RP_MODE) == RpMode.PASSIVE) return;

            Bitmap brandIcon = model.get(HeaderProperties.RP_BRAND_ICON);
            ImageView headerIconView = view.findViewById(R.id.header_rp_icon);
            ImageView arrowRangeIcon = view.findViewById(R.id.arrow_range_icon);
            if (brandIcon != null) {
                int iconSize =
                        resources.getDimensionPixelSize(
                                R.dimen.account_selection_active_mode_sheet_icon_size);
                Drawable croppedBrandIcon =
                        createBitmapWithMaskableIconSafeZone(resources, brandIcon, iconSize);
                headerIconView.setImageDrawable(croppedBrandIcon);
            }
            boolean isRpIconVisible =
                    brandIcon != null
                            && model.get(HeaderProperties.HEADER_ICON) != null
                            && model.get(HeaderProperties.TYPE)
                                    == HeaderProperties.HeaderType.REQUEST_PERMISSION_MODAL;
            headerIconView.setVisibility(isRpIconVisible ? View.VISIBLE : View.GONE);
            arrowRangeIcon.setVisibility(isRpIconVisible ? View.VISIBLE : View.GONE);
        } else if (key == HeaderProperties.CLOSE_ON_CLICK_LISTENER) {
            // There is no explicit close button for active mode, user swipes to close instead.
            if (model.get(HeaderProperties.RP_MODE) == RpMode.ACTIVE) return;

            final Runnable closeOnClickRunnable =
                    (Runnable) model.get(HeaderProperties.CLOSE_ON_CLICK_LISTENER);
            view.findViewById(R.id.close_button)
                    .setOnClickListener(
                            clickedView -> {
                                closeOnClickRunnable.run();
                            });
        } else {
            assert false : "Unhandled update to property:" + key;
        }
    }

    /** Returns text for the {@link HeaderType.VERIFY} header. */
    static @StringRes int getVerifyHeaderStringId() {
        return R.string.verify_sheet_title;
    }

    /** Returns text for the {@link HeaderType.VERIFY_AUTO_REAUTHN} header. */
    static @StringRes int getVerifyHeaderAutoReauthnStringId() {
        return R.string.verify_sheet_title_auto_reauthn;
    }

    private static String computeHeaderTitle(
            Resources resources,
            HeaderProperties.HeaderType type,
            String rpUrl,
            String idpUrl,
            @RpContext.EnumType int rpContext,
            @RpMode.EnumType int rpMode,
            Boolean isMultipleIdps) {
        @StringRes int titleStringId;
        // In single IDP active mode, show the title with RP and IDP.
        if (rpMode == RpMode.ACTIVE && !isMultipleIdps) {
            switch (rpContext) {
                case RpContext.SIGN_UP:
                    titleStringId =
                            R.string.account_selection_button_mode_sheet_title_explicit_signup;
                    break;
                case RpContext.USE:
                    titleStringId = R.string.account_selection_button_mode_sheet_title_explicit_use;
                    break;
                case RpContext.CONTINUE:
                    titleStringId =
                            R.string.account_selection_button_mode_sheet_title_explicit_continue;
                    break;
                default:
                    titleStringId =
                            R.string.account_selection_button_mode_sheet_title_explicit_signin;
            }
            return String.format(resources.getString(titleStringId), idpUrl);
        }

        // In passive mode, we change the title when signing in the user.
        if (rpMode == RpMode.PASSIVE && type == HeaderProperties.HeaderType.VERIFY) {
            return resources.getString(getVerifyHeaderStringId());
        }
        if (rpMode == RpMode.PASSIVE && type == HeaderProperties.HeaderType.VERIFY_AUTO_REAUTHN) {
            return resources.getString(getVerifyHeaderAutoReauthnStringId());
        }

        // If there are multiple IDPs, show the title with just the RP.
        if (isMultipleIdps) {
            // The title does not change depending on RP context in a dialog involving multiple
            // IDPs. Note that context is indeed shown when the dialog is transitioned to single
            // IDP, e.g. once the user selects an account.
            return String.format(
                    resources.getString(
                            R.string.account_selection_multi_idp_sheet_title_explicit_signin),
                    rpUrl);
        }

        switch (rpContext) {
            case RpContext.SIGN_UP:
                titleStringId = R.string.account_selection_sheet_title_explicit_signup;
                break;
            case RpContext.USE:
                titleStringId = R.string.account_selection_sheet_title_explicit_use;
                break;
            case RpContext.CONTINUE:
                titleStringId = R.string.account_selection_sheet_title_explicit_continue;
                break;
            default:
                titleStringId = R.string.account_selection_sheet_title_explicit_signin;
        }
        return String.format(resources.getString(titleStringId), rpUrl, idpUrl);
    }

    private static String computeHeaderSubtitle(
            Resources resources,
            String rpUrl,
            @RpMode.EnumType int rpMode,
            Boolean isMultipleAccountChooser,
            Boolean isMultipleIdps) {
        if (rpMode == RpMode.PASSIVE || isMultipleIdps) return "";

        if (isMultipleAccountChooser) {
            return String.format(
                    resources.getString(
                            R.string.account_selection_button_mode_sheet_choose_an_account),
                    rpUrl);
        }
        return rpUrl;
    }

    private AccountSelectionViewBinder() {}
}
