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
import android.widget.ImageView;
import android.widget.TextView;

import androidx.annotation.StringRes;

import com.google.android.material.color.MaterialColors;

import org.chromium.base.Callback;
import org.chromium.blink.mojom.RpContext;
import org.chromium.blink.mojom.RpMode;
import org.chromium.chrome.browser.ui.android.webid.AccountSelectionProperties.AccountProperties;
import org.chromium.chrome.browser.ui.android.webid.AccountSelectionProperties.AddAccountButtonProperties;
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
import org.chromium.components.browser_ui.widget.TintedDrawable;
import org.chromium.content.webid.IdentityRequestDialogDisclosureField;
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
                int avatarMonogramTextSize =
                        view.getResources()
                                .getDimensionPixelSize(
                                        R.dimen
                                                .account_selection_account_avatar_monogram_text_size);
                // TODO(crbug.com/40214151): Consult UI team to determine the background color we
                // need to use here.
                RoundedIconGenerator roundedIconGenerator =
                        new RoundedIconGenerator(
                                resources,
                                /* iconWidthDp= */ avatarSize,
                                /* iconHeightDp= */ avatarSize,
                                /* cornerRadiusDp= */ avatarSize / 2,
                                /* backgroundColor= */ Color.GRAY,
                                avatarMonogramTextSize);
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
                view.setOnClickListener(
                        clickedView -> {
                            clickCallback.onResult(account);
                        });
            }
        } else if (key == AccountProperties.ACCOUNT) {
            TextView name = view.findViewById(R.id.title);
            // Name is not shown in the account chip of the request permission dialog. The name is
            // shown in the Continue button instead.
            if (name != null) {
                name.setText(account.getName());
            }
            TextView email = view.findViewById(R.id.description);
            email.setText(account.getEmail());
        } else {
            assert false : "Unhandled update to property:" + key;
        }
    }

    /**
     * Called whenever an add account button is bound to this view.
     *
     * @param model The model containing the data for the view.
     * @param view The view to be bound.
     * @param key The key of the property to be bound.
     */
    @SuppressWarnings("checkstyle:SetTextColorAndSetTextSizeCheck")
    static void bindAddAccountView(PropertyModel model, View view, PropertyKey key) {
        if (key == AddAccountButtonProperties.PROPERTIES) {
            AddAccountButtonProperties.Properties properties =
                    model.get(AddAccountButtonProperties.PROPERTIES);
            Context context = view.getContext();

            // If iconView is available, the add account button is an account row at the end of the
            // accounts list.
            ImageView iconView = view.findViewById(R.id.start_icon);
            if (iconView != null) {
                TintedDrawable plusIcon =
                        TintedDrawable.constructTintedDrawable(
                                context,
                                R.drawable.plus,
                                R.color.default_icon_color_accent1_tint_list);
                iconView.setImageDrawable(plusIcon);

                TextView subject = view.findViewById(R.id.title);
                subject.setText(context.getString(R.string.account_selection_add_account));

                view.setOnClickListener(
                        clickedView -> {
                            properties.mOnClickListener.onResult(null);
                        });
                return;
            }

            // Since iconView is not available, the add account button is a secondary button under
            // the continue button at the bottom of the screen.
            ButtonCompat button = view.findViewById(R.id.account_selection_add_account_btn);
            button.setOnClickListener(
                    clickedView -> {
                        properties.mOnClickListener.onResult(null);
                    });
            button.setText(context.getString(R.string.account_selection_add_account));

            IdentityProviderMetadata idpMetadata = properties.mIdpMetadata;
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

    private static class ErrorText {
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
                            new NoUnderlineClickableSpan(
                                    context, (View clickedView) -> runnable.run()));
            mDescription = SpanApplier.applySpans(description, moreDetailsSpan);
        }
    }

    /**
     * Returns text to be displayed on the error dialog.
     * @param view The view to be bound.
     * @param properties The properties which determine what error text to display.
     * @return The ErrorText containing the summary and description to display.
     */
    private static ErrorText getErrorText(View view, ErrorProperties.Properties properties) {
        String code = properties.mError.getCode();
        GURL url = properties.mError.getUrl();
        String idpForDisplay = properties.mIdpForDisplay;
        String rpForDisplay = properties.mRpForDisplay;
        Context context = view.getContext();

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

            if (url.isEmpty()) {
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
                                ? R.string.signin_error_dialog_more_details_retry_prompt
                                : R.string.signin_error_dialog_more_details_prompt,
                        idpForDisplay);
        return new ErrorText(summary, description, context, properties.mMoreDetailsClickRunnable);
    }

    /**
     * Called whenever error text is bound to this view.
     * @param model The model containing the data for the view.
     * @param view The view to be bound.
     * @param key The key of the property to be bound.
     */
    static void bindErrorTextView(PropertyModel model, View view, PropertyKey key) {
        if (key == ErrorProperties.PROPERTIES) {
            ErrorText errorText = getErrorText(view, model.get(ErrorProperties.PROPERTIES));

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
                        properties.mOnClickListener.onResult(account);
                    });

            String btnText;
            HeaderProperties.HeaderType headerType = properties.mHeaderType;
            if (headerType == HeaderProperties.HeaderType.SIGN_IN_TO_IDP_STATIC) {
                btnText = context.getString(R.string.idp_signin_status_mismatch_dialog_continue);
            } else if (headerType == HeaderProperties.HeaderType.SIGN_IN_ERROR) {
                btnText = context.getString(R.string.signin_error_dialog_got_it_button);
            } else {
                // Prefers to use given name if it is provided otherwise falls back to using the
                // name.
                String givenName = account.getGivenName();
                String displayedName =
                        givenName != null && !givenName.isEmpty() ? givenName : account.getName();
                btnText =
                        String.format(
                                context.getString(R.string.account_selection_continue),
                                displayedName);
                button.setContentDescription(btnText + ", " + account.getEmail());
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
            String buttonText = context.getString(textId);
            button.setText(buttonText);
            if (!ColorUtils.inNightMode(context)) {
                IdentityProviderMetadata idpMetadata =
                        model.get(ErrorButtonProperties.IDP_METADATA);

                // TODO(crbug.com/40282202): Decide on how to set colours for error buttons.
                Integer textColor = idpMetadata.getBrandBackgroundColor();
                button.setTextColor(
                        textColor != null
                                ? textColor
                                : MaterialColors.getColor(context, R.attr.colorOnPrimary, TAG));
            }
        } else if (key == ErrorButtonProperties.ON_CLICK_LISTENER) {
            button.setOnClickListener(
                    clickedView -> {
                        model.get(ErrorButtonProperties.ON_CLICK_LISTENER).run();
                    });
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
            itemBinder = AccountSelectionViewBinder::bindAddAccountView;
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
                || key == HeaderProperties.SET_FOCUS_VIEW_CALLBACK) {
            TextView headerTitleText = view.findViewById(R.id.header_title);
            TextView headerSubtitleText = view.findViewById(R.id.header_subtitle);
            HeaderProperties.HeaderType headerType = model.get(HeaderProperties.TYPE);

            String subtitle =
                    computeHeaderSubtitle(
                            resources,
                            model.get(HeaderProperties.RP_FOR_DISPLAY),
                            model.get(HeaderProperties.RP_MODE),
                            model.get(HeaderProperties.IS_MULTIPLE_ACCOUNT_CHOOSER));
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
                            model.get(HeaderProperties.RP_MODE));
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
        } else if (key == HeaderProperties.IDP_BRAND_ICON) {
            Bitmap brandIcon = model.get(HeaderProperties.IDP_BRAND_ICON);
            if (brandIcon != null) {
                int iconSize =
                        resources.getDimensionPixelSize(
                                model.get(HeaderProperties.RP_MODE) == RpMode.ACTIVE
                                        ? R.dimen.account_selection_button_mode_sheet_icon_size
                                        : R.dimen.account_selection_sheet_icon_size);
                Drawable croppedBrandIcon =
                        createBitmapWithMaskableIconSafeZone(resources, brandIcon, iconSize);
                ImageView headerIconView = (ImageView) view.findViewById(R.id.header_idp_icon);
                headerIconView.setImageDrawable(croppedBrandIcon);
                headerIconView.setVisibility(View.VISIBLE);
            }
        } else if (key == HeaderProperties.RP_BRAND_ICON) {
            // RP icon is not shown in passive mode.
            if (model.get(HeaderProperties.RP_MODE) == RpMode.PASSIVE) return;

            Bitmap brandIcon = model.get(HeaderProperties.RP_BRAND_ICON);
            ImageView headerIconView = (ImageView) view.findViewById(R.id.header_rp_icon);
            ImageView arrowRangeIcon = (ImageView) view.findViewById(R.id.arrow_range_icon);
            if (brandIcon != null) {
                int iconSize =
                        resources.getDimensionPixelSize(
                                R.dimen.account_selection_button_mode_sheet_icon_size);
                Drawable croppedBrandIcon =
                        createBitmapWithMaskableIconSafeZone(resources, brandIcon, iconSize);
                headerIconView.setImageDrawable(croppedBrandIcon);
            }
            boolean isRpIconVisible =
                    brandIcon != null
                            && model.get(HeaderProperties.IDP_BRAND_ICON) != null
                            && model.get(HeaderProperties.TYPE)
                                    == HeaderProperties.HeaderType.REQUEST_PERMISSION;
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
            @RpMode.EnumType int rpMode) {
        @StringRes int titleStringId;
        if (rpMode == RpMode.ACTIVE) {
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

        if (type == HeaderProperties.HeaderType.VERIFY) {
            return resources.getString(getVerifyHeaderStringId());
        }
        if (type == HeaderProperties.HeaderType.VERIFY_AUTO_REAUTHN) {
            return resources.getString(getVerifyHeaderAutoReauthnStringId());
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
            Boolean isMultipleAccountChooser) {
        if (rpMode == RpMode.PASSIVE) return "";

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
