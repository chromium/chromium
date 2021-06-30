// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.android.webid;

import android.content.res.Resources;
import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Paint;
import android.graphics.PorterDuff;
import android.graphics.PorterDuffXfermode;
import android.graphics.drawable.BitmapDrawable;
import android.graphics.drawable.Drawable;
import android.view.View;
import android.widget.Button;
import android.widget.ImageView;
import android.widget.TextView;

import androidx.annotation.StringRes;

import org.chromium.chrome.browser.ui.android.webid.AccountSelectionProperties.AccountProperties;
import org.chromium.chrome.browser.ui.android.webid.AccountSelectionProperties.ContinueButtonProperties;
import org.chromium.chrome.browser.ui.android.webid.AccountSelectionProperties.HeaderProperties;
import org.chromium.chrome.browser.ui.android.webid.data.Account;
import org.chromium.chrome.browser.ui.favicon.FaviconUtils;
import org.chromium.components.browser_ui.util.AvatarGenerator;
import org.chromium.components.browser_ui.widget.RoundedIconGenerator;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * Provides functions that map {@link AccountSelectionProperties} changes in a {@link PropertyModel}
 * to the suitable method in {@link AccountSelectionView}.
 */
class AccountSelectionViewBinder {
    private static RoundedIconGenerator sRoundedIconGenerator;

    static RoundedIconGenerator getRoundedIconGenerator(Resources resources) {
        if (sRoundedIconGenerator == null) {
            sRoundedIconGenerator = FaviconUtils.createCircularIconGenerator(resources);
        }
        return sRoundedIconGenerator;
    }

    /**
     * Called whenever an account is bound to this view.
     * @param model The model containing the data for the view.
     * @param view The view to be bound.
     * @param key The key of the property to be bound.
     */
    static void bindAccountView(PropertyModel model, View view, PropertyKey key) {
        Account account = model.get(AccountProperties.ACCOUNT);
        if (key == AccountProperties.AVATAR || key == AccountProperties.FAVICON_OR_FALLBACK) {
            AccountProperties.Avatar avatarData = model.get(AccountProperties.AVATAR);
            AccountProperties.FaviconOrFallback faviconData =
                    model.get(AccountProperties.FAVICON_OR_FALLBACK);
            // Wait for both avatar and favicon to be available before drawing them to avoid
            // unnecessary flashing.
            if (avatarData == null || faviconData == null) return;
            Drawable badgedAvatar = overlayIdpFaviconOnAvatar(view, avatarData, faviconData);
            ImageView avatarView = view.findViewById(R.id.start_icon);
            avatarView.setImageDrawable(badgedAvatar);
        } else if (key == AccountProperties.ON_CLICK_LISTENER) {
            view.setOnClickListener(clickedView -> {
                model.get(AccountProperties.ON_CLICK_LISTENER).onResult(account);
            });
        } else if (key == AccountProperties.ACCOUNT) {
            TextView subject = view.findViewById(R.id.title);
            subject.setText(account.getName());
            TextView email = view.findViewById(R.id.description);
            email.setText(account.getEmail());
        } else {
            assert false : "Unhandled update to property:" + key;
        }
    }

    /**
     * Creates a drawable that is a combination of the User's avatar and the Identity Provider's
     * (IdP) favicon. The final drawable is a square with avatarData.mAvatarSize dimension that
     * contains two rounded images overlayed on top of each other like so:
     * +------------+
     * |            |
     * |   Avatar   |
     * |       +----+
     * |       |Favi|
     * |       |con |
     * +-------+----+
     *
     * @param view The view to be bound.
     * @param avatarData The data for the avatar. If the bitmap is null then we generate a
     *                   placeholder monogram avatar using the name.
     * @param faviconData The data for the favicon including its bitmap and size.
     */
    static Drawable overlayIdpFaviconOnAvatar(View view, AccountProperties.Avatar avatarData,
            AccountProperties.FaviconOrFallback faviconData) {
        int avatarSize = avatarData.mAvatarSize;
        int badgeSize = faviconData.mIconSize;
        int frameSize = avatarSize;
        // Avatar touches the top/left sides of frame, badge touches bottom/right sides of the
        // frame.
        int avatarX = 0;
        int avatarY = 0;
        int badgeX = frameSize - badgeSize;
        int badgeY = frameSize - badgeSize;

        RoundedIconGenerator roundedIconGenerator = getRoundedIconGenerator(view.getResources());

        // Prepare avatar or its fallback monogram.
        Bitmap avatar = avatarData.mAvatar;
        if (avatar == null) {
            // TODO(majidvp): Consult UI team to determine the background color we need to use here.
            roundedIconGenerator.setBackgroundColor(Color.GRAY);
            avatar = roundedIconGenerator.generateIconForText(avatarData.mName);
        }
        Drawable croppedAvatar =
                AvatarGenerator.makeRoundAvatar(view.getResources(), avatar, avatarSize);
        Bitmap badgedAvatar = Bitmap.createBitmap(avatarSize, avatarSize, Bitmap.Config.ARGB_8888);
        Canvas canvas = new Canvas(badgedAvatar);

        // Draw the avatar.
        croppedAvatar.setBounds(avatarX, avatarY, avatarSize, avatarSize);
        croppedAvatar.draw(canvas);

        // Cut a transparent hole through the avatar image. This will serve as a border to the badge
        // being overlaid.
        int badgeRadius = badgeSize / 2;
        int badgeCenterX = badgeX + badgeRadius;
        int badgeCenterY = badgeY + badgeRadius;
        int badgeBorderSize = view.getResources().getDimensionPixelSize(
                R.dimen.account_selection_favicon_border_size);

        Paint paint = new Paint();
        paint.setAntiAlias(true);
        paint.setXfermode(new PorterDuffXfermode(PorterDuff.Mode.CLEAR));
        canvas.drawCircle(badgeCenterX, badgeCenterY, badgeRadius + badgeBorderSize, paint);

        // Prepare the IDP favicon as the badge.
        Drawable favicon = FaviconUtils.getIconDrawableWithoutFilter(faviconData.mIcon,
                faviconData.mUrl, faviconData.mFallbackColor, roundedIconGenerator,
                view.getResources(), badgeSize);

        // Draw the badge.
        favicon.setBounds(badgeX, badgeY, badgeX + badgeSize, badgeY + badgeSize);
        favicon.draw(canvas);
        return new BitmapDrawable(view.getResources(), badgedAvatar);
    }

    /**
     * Called whenever a continue button for a single account is bound to this view.
     * @param model The model containing the data for the view.
     * @param view The view to be bound.
     * @param key The key of the property to be bound.
     */
    static void bindContinueButtonView(PropertyModel model, View view, PropertyKey key) {
        if (key == ContinueButtonProperties.ON_CLICK_LISTENER
                || key == ContinueButtonProperties.ACCOUNT) {
            Account account = model.get(ContinueButtonProperties.ACCOUNT);
            view.setOnClickListener(clickedView -> {
                model.get(ContinueButtonProperties.ON_CLICK_LISTENER).onResult(account);
            });
            // Prefers to use given name if it is provided otherwise falls back to using the name.
            String name =
                    account.getGivenName() != null ? account.getGivenName() : account.getName();
            String btnText = String.format(
                    view.getContext().getString(R.string.account_selection_continue), name);
            Button button = view.findViewById(R.id.account_selection_continue_btn);
            button.setText(btnText);
        } else {
            assert false : "Unhandled update to property:" + key;
        }
    }

    /**
     * Called whenever a header is bound to this view.
     * @param model The model containing the data for the view.
     * @param view The view to be bound.
     * @param key The key of the property to be bound.
     */
    static void bindHeaderView(PropertyModel model, View view, PropertyKey key) {
        if (key == HeaderProperties.SINGLE_ACCOUNT || key == HeaderProperties.FORMATTED_URL) {
            TextView sheetTitleText = view.findViewById(R.id.account_selection_sheet_title);
            @StringRes
            int titleStringId;
            if (model.get(HeaderProperties.SINGLE_ACCOUNT)) {
                titleStringId = R.string.account_selection_sheet_title_single;
            } else {
                titleStringId = R.string.account_selection_sheet_title;
            }

            String title = String.format(view.getContext().getString(titleStringId),
                    model.get(HeaderProperties.FORMATTED_URL));
            sheetTitleText.setText(title);
        } else {
            assert false : "Unhandled update to property:" + key;
        }
    }

    private AccountSelectionViewBinder() {}
}
