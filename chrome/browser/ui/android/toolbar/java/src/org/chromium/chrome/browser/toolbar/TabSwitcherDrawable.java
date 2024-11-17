// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar;

import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.ColorFilter;
import android.graphics.Paint;
import android.graphics.Paint.Align;
import android.graphics.PorterDuff;
import android.graphics.PorterDuffXfermode;
import android.graphics.Rect;
import android.graphics.Typeface;
import android.text.TextPaint;

import androidx.annotation.IntDef;

import org.chromium.base.ObserverList;
import org.chromium.chrome.browser.theme.ThemeUtils;
import org.chromium.chrome.browser.ui.theme.BrandedColorScheme;
import org.chromium.components.browser_ui.styles.SemanticColorUtils;
import org.chromium.components.browser_ui.widget.TintedDrawable;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.Locale;

/** A drawable for the tab switcher icon. */
public class TabSwitcherDrawable extends TintedDrawable {
    @IntDef({
        TabSwitcherDrawableLocation.TAB_TOOLBAR,
        TabSwitcherDrawableLocation.HUB_TOOLBAR,
        TabSwitcherDrawableLocation.TAB_SWITCHER_TOOLBAR,
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface TabSwitcherDrawableLocation {
        int TAB_TOOLBAR = 0;
        int HUB_TOOLBAR = 1;
        int TAB_SWITCHER_TOOLBAR = 2;
    }

    /** Observer interface for consumers who wish to subscribe to updates of TabSwitcherDrawable. */
    public interface Observer {
        default void onDrawableStateChanged() {}
    }

    private static final float LEFT_FRACTION = 3f / 4f;

    private final float mSingleDigitTextSize;
    private final float mDoubleDigitTextSize;

    private final Rect mTextBounds = new Rect();
    private final Paint mNotificationPaint;
    private final Paint mNotificationBgPaint;
    private final TextPaint mTextPaint;
    private final Paint mIconPaint;

    // Tab Count Label
    private int mTabCount;
    private boolean mIncognito;
    private String mTextRenderedForTesting;
    private Canvas mIconCanvas;
    private Bitmap mIconBitmap;
    private boolean mShouldShowNotificationIcon;
    private @TabSwitcherDrawableLocation int mTabSwitcherDrawableLocation;
    private ObserverList<Observer> mTabSwitcherDrawableObservers = new ObserverList<>();

    /**
     * Creates a {@link TabSwitcherDrawable}.
     *
     * @param context A {@link Context} instance.
     * @param brandedColorScheme The {@link BrandedColorScheme} used to tint the drawable.
     * @param bitmap The icon represented as a bitmap to be shown with this drawable.
     * @param tabSwitcherDrawableLocation The location in which the drawable is used.
     * @return A {@link TabSwitcherDrawable} instance.
     */
    public static TabSwitcherDrawable createTabSwitcherDrawable(
            Context context,
            @BrandedColorScheme int brandedColorScheme,
            @TabSwitcherDrawableLocation int tabSwitcherDrawableLocation) {
        Bitmap icon =
                BitmapFactory.decodeResource(
                        context.getResources(), R.drawable.btn_tabswitcher_modern);
        return new TabSwitcherDrawable(
                context, brandedColorScheme, icon, tabSwitcherDrawableLocation);
    }

    private TabSwitcherDrawable(
            Context context,
            @BrandedColorScheme int brandedColorScheme,
            Bitmap bitmap,
            @TabSwitcherDrawableLocation int tabSwitcherDrawableLocation) {
        super(context, bitmap);
        setTint(ThemeUtils.getThemedToolbarIconTint(context, brandedColorScheme));
        mSingleDigitTextSize =
                context.getResources().getDimension(R.dimen.toolbar_tab_count_text_size_1_digit);
        mDoubleDigitTextSize =
                context.getResources().getDimension(R.dimen.toolbar_tab_count_text_size_2_digit);

        mTextPaint = new TextPaint();
        mTextPaint.setAntiAlias(true);
        mTextPaint.setTextAlign(Align.CENTER);
        mTextPaint.setTypeface(Typeface.create("google-sans-medium", Typeface.BOLD));
        mTextPaint.setColor(getColorForState());

        mIconPaint = new Paint();
        mIconPaint.setAntiAlias(true);

        mNotificationBgPaint = new Paint();
        mNotificationBgPaint.setAntiAlias(true);
        mNotificationBgPaint.setStyle(Paint.Style.FILL);
        mNotificationBgPaint.setColor(Color.TRANSPARENT);
        mNotificationBgPaint.setXfermode(new PorterDuffXfermode(PorterDuff.Mode.CLEAR));

        mNotificationPaint = new Paint();
        mNotificationPaint.setAntiAlias(true);
        mNotificationPaint.setStyle(Paint.Style.FILL);
        mNotificationPaint.setColor(SemanticColorUtils.getDefaultIconColorAccent1(context));

        // Draw all icon components onto a local canvas before setting on the actual canvas.
        mIconBitmap =
                Bitmap.createBitmap(bitmap.getWidth(), bitmap.getHeight(), Bitmap.Config.ARGB_8888);
        mIconCanvas = new Canvas(mIconBitmap);
        mTabSwitcherDrawableLocation = tabSwitcherDrawableLocation;
    }

    @Override
    protected boolean onStateChange(int[] state) {
        boolean retVal = super.onStateChange(state);
        if (retVal) mTextPaint.setColor(getColorForState());
        return retVal;
    }

    @Override
    public void draw(Canvas canvas) {
        // Clear the canvas on each redraw.
        mIconCanvas.drawColor(Color.TRANSPARENT, PorterDuff.Mode.CLEAR);
        super.draw(mIconCanvas);

        Rect drawableBounds = getBounds();
        String textString = getTabCountString();
        mTextRenderedForTesting = textString;
        if (!textString.isEmpty()) {
            mTextPaint.getTextBounds(textString, 0, textString.length(), mTextBounds);

            int textX = drawableBounds.width() / 2;
            int textY =
                    drawableBounds.height() / 2
                            + (mTextBounds.bottom - mTextBounds.top) / 2
                            - mTextBounds.bottom;

            mIconCanvas.drawText(textString, textX, textY, mTextPaint);
        }

        // Do not show the notification icon in tab view incognito.
        if (mShouldShowNotificationIcon && shouldShowNotificationOnIncognito()) {
            // Draw the notification bubble.
            float ringWidth = drawableBounds.width() / 18f;
            float ringHeight = drawableBounds.height() / 18f;
            float notifFillLeft = (drawableBounds.width() * LEFT_FRACTION) - ringWidth;
            float notifFillBottom = (drawableBounds.height() / 4f) + ringHeight;
            float notifBgLeft = (drawableBounds.width() * LEFT_FRACTION) - (ringWidth * 2f);
            float notifBgBottom = (drawableBounds.height() / 4f) + (ringHeight * 2f);
            mIconCanvas.drawOval(
                    notifBgLeft,
                    drawableBounds.top,
                    drawableBounds.right,
                    notifBgBottom,
                    mNotificationBgPaint);
            mIconCanvas.drawOval(
                    notifFillLeft,
                    drawableBounds.top + ringHeight,
                    drawableBounds.right - ringWidth,
                    notifFillBottom,
                    mNotificationPaint);
        }

        canvas.drawBitmap(mIconBitmap, 0, 0, mIconPaint);
    }

    /**
     * @return The current tab count this drawable is displaying.
     */
    public int getTabCount() {
        return mTabCount;
    }

    /** Add on observer for when the drawable state changes. */
    public void addTabSwitcherDrawableObserver(Observer observer) {
        mTabSwitcherDrawableObservers.addObserver(observer);
    }

    /** Remove the observer for when the drawable state changes. */
    public void removeTabSwitcherDrawableObserver(Observer observer) {
        mTabSwitcherDrawableObservers.removeObserver(observer);
    }

    /**
     * Update the visual state based on the number of tabs present.
     *
     * @param tabCount The number of tabs.
     */
    public void updateForTabCount(int tabCount, boolean incognito) {
        if (tabCount == mTabCount && incognito == mIncognito) return;
        mTabCount = tabCount;
        mIncognito = incognito;
        float textSizePx = mTabCount > 9 ? mDoubleDigitTextSize : mSingleDigitTextSize;
        mTextPaint.setTextSize(textSizePx);
        invalidateSelf();

        for (Observer observer : mTabSwitcherDrawableObservers) {
            observer.onDrawableStateChanged();
        }
    }

    private String getTabCountString() {
        if (mTabCount <= 0) {
            return "";
        } else if (mTabCount > 99) {
            return mIncognito ? ";)" : ":D";
        } else {
            return String.format(Locale.getDefault(), "%d", mTabCount);
        }
    }

    private int getColorForState() {
        return mTint.getColorForState(getState(), 0);
    }

    private void updatePaint() {
        if (mTextPaint != null) mTextPaint.setColor(getColorForState());
    }

    @Override
    public void setColorFilter(ColorFilter colorFilter) {
        super.setColorFilter(colorFilter);
        updatePaint();
    }

    /**
     * This call is responsible for setting whether the notification bubble shows on the icon or
     * not. Any non-test callsite should be guarded by the DATA_SHARING flag.
     */
    public void setNotificationIconStatus(boolean shouldShow) {
        if (mShouldShowNotificationIcon == shouldShow) return;
        mShouldShowNotificationIcon = shouldShow;
        invalidateSelf();

        for (Observer observer : mTabSwitcherDrawableObservers) {
            observer.onDrawableStateChanged();
        }
    }

    /** Returns whether the drawable should show a notification icon. */
    public boolean getShowIconNotificationStatus() {
        return mShouldShowNotificationIcon;
    }

    public void setNotificationBackground(@BrandedColorScheme int brandedColorScheme) {
        if (brandedColorScheme == BrandedColorScheme.LIGHT_BRANDED_THEME
                || brandedColorScheme == BrandedColorScheme.DARK_BRANDED_THEME) {
            mNotificationBgPaint.setColor(Color.WHITE);
            mNotificationBgPaint.setXfermode(null);
        } else {
            mNotificationBgPaint.setColor(Color.TRANSPARENT);
            mNotificationBgPaint.setXfermode(new PorterDuffXfermode(PorterDuff.Mode.CLEAR));
        }
    }

    public void setIncognitoStatus(boolean isIncognito) {
        mIncognito = isIncognito;
    }

    private boolean shouldShowNotificationOnIncognito() {
        return !(mIncognito
                && mTabSwitcherDrawableLocation == TabSwitcherDrawableLocation.TAB_TOOLBAR);
    }

    public String getTextRenderedForTesting() {
        return mTextRenderedForTesting;
    }
}
