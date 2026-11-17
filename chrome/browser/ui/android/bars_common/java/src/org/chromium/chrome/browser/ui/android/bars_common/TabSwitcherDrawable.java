// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.android.bars_common;

import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.ColorFilter;
import android.graphics.Paint;
import android.graphics.Paint.Align;
import android.graphics.Path;
import android.graphics.PorterDuff;
import android.graphics.PorterDuffXfermode;
import android.graphics.Rect;
import android.graphics.RectF;
import android.graphics.Typeface;
import android.text.TextPaint;

import androidx.annotation.IntDef;

import org.chromium.base.ObserverList;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.theme.ThemeUtils;
import org.chromium.chrome.browser.ui.theme.BrandedColorScheme;
import org.chromium.components.browser_ui.styles.SemanticColorUtils;
import org.chromium.components.browser_ui.widget.TintedDrawable;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.Locale;

/** A drawable for the tab switcher icon. */
@NullMarked
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
    // Horizontal centering target for the label: 0 is the ink bounding-box center, 1 is the ink
    // center of mass. The value was picked by eye on Google Sans renders.
    private static final float OPTICAL_CENTER_X_WEIGHT = 0.8f;
    // Maximum deviation allowed when flattening glyph outlines into line segments.
    private static final float OUTLINE_TOLERANCE_PX = 0.25f;

    private final float mSingleDigitTextSize;
    private final float mDoubleDigitTextSize;

    private final Rect mTextBounds = new Rect();
    private final Paint mNotificationPaint;
    private final Paint mNotificationBgPaint;
    private final TextPaint mTextPaint;

    // Tab Count Label
    private int mTabCount;
    private boolean mIncognito;
    private @Nullable String mTextRenderedForTesting;
    private @Nullable String mMeasuredText;
    private float mOpticalCenterOffsetX;
    private float mOpticalCenterOffsetY;
    private boolean mShouldShowNotificationIcon;
    private final @TabSwitcherDrawableLocation int mTabSwitcherDrawableLocation;
    private final ObserverList<Observer> mTabSwitcherDrawableObservers = new ObserverList<>();

    /**
     * Creates a {@link TabSwitcherDrawable}.
     *
     * @param context A {@link Context} instance.
     * @param brandedColorScheme The {@link BrandedColorScheme} used to tint the drawable.
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
        mTextPaint.setSubpixelText(true);
        mTextPaint.setTextAlign(Align.LEFT);
        mTextPaint.setTypeface(Typeface.create("google-sans-medium", Typeface.BOLD));
        mTextPaint.setColor(getColorForState());

        mNotificationBgPaint = new Paint();
        mNotificationBgPaint.setAntiAlias(true);
        mNotificationBgPaint.setStyle(Paint.Style.FILL);
        mNotificationBgPaint.setColor(Color.TRANSPARENT);
        mNotificationBgPaint.setXfermode(new PorterDuffXfermode(PorterDuff.Mode.CLEAR));

        mNotificationPaint = new Paint();
        mNotificationPaint.setAntiAlias(true);
        mNotificationPaint.setStyle(Paint.Style.FILL);
        mNotificationPaint.setColor(SemanticColorUtils.getDefaultIconColorAccent1(context));

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
        Rect drawableBounds = getBounds();
        boolean showNotification =
                mShouldShowNotificationIcon && shouldShowNotificationOnIncognito();

        if (showNotification) {
            // saveLayer is required to isolate the CLEAR blend mode of the notification ring
            // so it doesn't clear the content behind the toolbar.
            canvas.saveLayer(
                    drawableBounds.left,
                    drawableBounds.top,
                    drawableBounds.right,
                    drawableBounds.bottom,
                    /* paint= */ null);
        } else {
            canvas.save();
        }

        // Draw base icon (scales automatically to drawable bounds)
        super.draw(canvas);

        String textString = getTabCountString();
        mTextRenderedForTesting = textString;
        if (!textString.isEmpty()) {
            updateOpticalCenterIfNeeded(textString);

            float textX = drawableBounds.exactCenterX() - mOpticalCenterOffsetX;
            // A whole-pixel baseline keeps every count equally sharp and at the same height.
            float textY = Math.round(drawableBounds.exactCenterY() - mOpticalCenterOffsetY);

            canvas.drawText(textString, textX, textY, mTextPaint);
        }

        // Do not show the notification icon in tab view incognito.
        if (showNotification) {
            // Draw the notification bubble.
            float ringWidth = drawableBounds.width() / 18f;
            float ringHeight = drawableBounds.height() / 18f;
            float notifFillLeft = (drawableBounds.width() * LEFT_FRACTION) - ringWidth;
            float notifFillBottom = (drawableBounds.height() / 4f) + ringHeight;
            float notifBgLeft = (drawableBounds.width() * LEFT_FRACTION) - (ringWidth * 2f);
            float notifBgBottom = (drawableBounds.height() / 4f) + (ringHeight * 2f);
            canvas.drawOval(
                    notifBgLeft,
                    drawableBounds.top,
                    drawableBounds.right,
                    notifBgBottom,
                    mNotificationBgPaint);
            canvas.drawOval(
                    notifFillLeft,
                    drawableBounds.top + ringHeight,
                    drawableBounds.right - ringWidth,
                    notifFillBottom,
                    mNotificationPaint);
        }

        canvas.restore();
    }

    /** Returns the current tab count this drawable is displaying. */
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
     * @param incognito Whether the drawable is shown for incognito.
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

    /**
     * Measures where the label's ink sits relative to its draw origin, so {@link #draw} can center
     * the ink rather than the font metrics.
     *
     * <p>The horizontal center of mass comes from the glyph outlines, so nothing is rasterized. The
     * result is cached by label only. That is valid because text size and typeface are fixed per
     * label; a change that varies either independently must also invalidate the cache.
     */
    private void updateOpticalCenterIfNeeded(String textString) {
        if (textString.equals(mMeasuredText)) return;
        mMeasuredText = textString;

        mTextPaint.getTextBounds(textString, /* start= */ 0, textString.length(), mTextBounds);
        mOpticalCenterOffsetX = mTextBounds.exactCenterX();
        mOpticalCenterOffsetY = mTextBounds.exactCenterY();

        // Counts share the vertical center of all ten digits, so the baseline does not move from
        // one count to the next. The overflow labels use their own.
        String verticalReference =
                mTabCount > 99 ? textString : String.format(Locale.getDefault(), "%d", 1234567890);
        Path path = new Path();
        RectF inkBounds = new RectF();
        mTextPaint.getTextPath(
                verticalReference,
                /* start= */ 0,
                verticalReference.length(),
                /* x= */ 0f,
                /* y= */ 0f,
                path);
        path.computeBounds(inkBounds, /* exact= */ true);
        // Fonts without outlines (e.g. bitmap fonts) keep the bounding-box center on both axes.
        if (inkBounds.isEmpty()) return;
        mOpticalCenterOffsetY = inkBounds.centerY();

        mTextPaint.getTextPath(
                textString, /* start= */ 0, textString.length(), /* x= */ 0f, /* y= */ 0f, path);
        // Flattened outlines as [fraction, x, y] triples. The shoelace formula over each closed
        // contour gives the filled area and its centroid, with holes subtracting.
        float[] points = path.approximate(OUTLINE_TOLERANCE_PX);
        double area = 0;
        double moment = 0;
        int contourStart = 0;
        for (int i = 3; i <= points.length; i += 3) {
            // A contour ends at the last point or at a zero-length jump (unchanged fraction) to
            // the next one. Its last edge returns to its own first point.
            boolean isContourEnd = i == points.length || points[i] == points[i - 3];
            int next = isContourEnd ? contourStart : i;
            float x0 = points[i - 2];
            float y0 = points[i - 1];
            float x1 = points[next + 1];
            float y1 = points[next + 2];
            double cross = (double) x0 * y1 - (double) x1 * y0;
            area += cross;
            moment += (x0 + x1) * cross;
            if (isContourEnd) contourStart = i;
        }
        if (area == 0) return;
        float massCenterX = (float) (moment / (3 * area));
        // A real centroid lies within the ink. Mixed contour winding (e.g. across fallback fonts)
        // can partially cancel the signed areas, so keep the bounding-box center instead.
        if (massCenterX < mTextBounds.left || massCenterX > mTextBounds.right) return;
        mOpticalCenterOffsetX += OPTICAL_CENTER_X_WEIGHT * (massCenterX - mOpticalCenterOffsetX);
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
        invalidateSelf();
    }

    private boolean shouldShowNotificationOnIncognito() {
        return !(mIncognito
                && mTabSwitcherDrawableLocation == TabSwitcherDrawableLocation.TAB_TOOLBAR);
    }

    public @Nullable String getTextRenderedForTesting() {
        return mTextRenderedForTesting;
    }
}
