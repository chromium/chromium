// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ephemeraltab;

import android.content.Context;
import android.graphics.drawable.Drawable;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewTreeObserver;
import android.widget.FrameLayout;
import android.widget.ImageView;
import android.widget.ProgressBar;
import android.widget.TextView;

import androidx.annotation.DrawableRes;
import androidx.annotation.Nullable;

import org.chromium.base.Callback;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.supplier.UnownedUserDataSupplier;
import org.chromium.chrome.browser.share.ShareDelegate;
import org.chromium.chrome.browser.share.ShareDelegateSupplier;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent;
import org.chromium.components.browser_ui.widget.ChromeTransitionDrawable;
import org.chromium.components.browser_ui.widget.FadingShadow;
import org.chromium.components.browser_ui.widget.FadingShadowView;
import org.chromium.components.embedder_support.delegate.WebContentsDelegateAndroid;
import org.chromium.components.embedder_support.view.ContentView;
import org.chromium.components.thinwebview.ThinWebView;
import org.chromium.components.thinwebview.ThinWebViewConstraints;
import org.chromium.components.thinwebview.ThinWebViewFactory;
import org.chromium.components.url_formatter.SchemeDisplay;
import org.chromium.components.url_formatter.UrlFormatter;
import org.chromium.content_public.browser.RenderCoordinates;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.IntentRequestTracker;
import org.chromium.ui.base.ViewUtils;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.url.GURL;

/**
 * Represents ephemeral tab content and the toolbar, which can be included inside the bottom sheet.
 */
public class EphemeralTabSheetContent implements BottomSheetContent {
    /**
     * The base duration of the settling animation of the sheet. 218 ms is a spec for material
     * design (this is the minimum time a user is guaranteed to pay attention to something).
     */
    private static final int BASE_ANIMATION_DURATION_MS = 218;

    /** Ratio of the height when in full mode. Used in half-open variation. */
    private static final float FULL_HEIGHT_RATIO = 0.9f;

    private final Context mContext;
    private final Runnable mOpenNewTabCallback;
    private final Runnable mToolbarClickCallback;
    private final Runnable mCloseButtonCallback;
    private final UnownedUserDataSupplier<ShareDelegate> mShareDelegateSupplier =
            new ShareDelegateSupplier();
    private final ObservableSupplierImpl<Boolean> mBackPressStateChangedSupplier =
            new ObservableSupplierImpl<>();
    private final Callback<ViewGroup> mOnToolbarCreatedCallback;

    private ViewGroup mToolbarView;
    private ViewGroup mSheetContentView;

    private WebContents mWebContents;
    private ContentView mWebContentView;
    private ThinWebView mThinWebView;
    private FadingShadowView mShadow;
    private Drawable mCurrentFavicon;
    private ImageView mFaviconView;

    /**
     * Constructor.
     * @param context An Android context.
     * @param openNewTabCallback Callback invoked to open a new tab.
     * @param toolbarClickCallback Callback invoked when user clicks on the toolbar.
     * @param closeButtonCallback Callback invoked when user clicks on the close button.
     * @param maxViewHeight The height of the sheet in full height position.
     * @param intentRequestTracker The {@link IntentRequestTracker} of the current activity.
     * @param onToolbarCreatedCallback Callback invoked to notify observers on toolbar creation.
     */
    public EphemeralTabSheetContent(
            Context context,
            Runnable openNewTabCallback,
            Runnable toolbarClickCallback,
            Runnable closeButtonCallback,
            int maxViewHeight,
            IntentRequestTracker intentRequestTracker,
            Callback<ViewGroup> onToolbarCreatedCallback) {
        mContext = context;
        mOpenNewTabCallback = openNewTabCallback;
        mToolbarClickCallback = toolbarClickCallback;
        mCloseButtonCallback = closeButtonCallback;
        mOnToolbarCreatedCallback = onToolbarCreatedCallback;

        createThinWebView(getMaxSheetHeight(maxViewHeight), intentRequestTracker);
        createToolbarView(maxViewHeight);

        mBackPressStateChangedSupplier.set(true);
    }

    /**
     * Add web contents to the sheet.
     * @param webContents The {@link WebContents} to be displayed.
     * @param contentView The {@link ContentView} associated with the web contents.
     * @param delegate The {@link WebContentsDelegateAndroid} that handles requests on WebContents.
     */
    public void attachWebContents(
            WebContents webContents, ContentView contentView, WebContentsDelegateAndroid delegate) {
        mWebContents = webContents;
        mWebContentView = contentView;
        if (mWebContentView.getParent() != null) {
            ((ViewGroup) mWebContentView.getParent()).removeView(mWebContentView);
        }
        mThinWebView.attachWebContents(mWebContents, mWebContentView, delegate);

        // Initialize the supplier of {@link ShareDelegate} for the WindowAndroid used by
        // ThinWebView.  The {@link ShareDelegate} itself is not set by design in order to leave
        // the share feature disabled on Preview Tab.
        WindowAndroid window = mWebContents.getTopLevelNativeWindow();
        assert window != null;
        mShareDelegateSupplier.attach(window.getUnownedUserDataHost());
    }

    /**
     * Create a ThinWebView, add it to the view hierarchy, which represents the contents of the
     * bottom sheet.
     */
    private void createThinWebView(int maxSheetHeight, IntentRequestTracker intentRequestTracker) {
        mThinWebView =
                ThinWebViewFactory.create(
                        mContext, new ThinWebViewConstraints(), intentRequestTracker);

        mSheetContentView = new FrameLayout(mContext);
        mThinWebView
                .getView()
                .setLayoutParams(
                        new FrameLayout.LayoutParams(
                                ViewGroup.LayoutParams.MATCH_PARENT, maxSheetHeight));
        mSheetContentView.addView(mThinWebView.getView());
    }

    private void createToolbarView(int maxViewHeight) {
        mToolbarView =
                (ViewGroup) LayoutInflater.from(mContext).inflate(R.layout.sheet_tab_toolbar, null);
        mShadow = mToolbarView.findViewById(R.id.shadow);
        mShadow.init(mContext.getColor(R.color.toolbar_shadow_color), FadingShadow.POSITION_TOP);
        ImageView openInNewTabButton = mToolbarView.findViewById(R.id.open_in_new_tab);
        openInNewTabButton.setOnClickListener(view -> mOpenNewTabCallback.run());

        mToolbarView.setOnClickListener(view -> mToolbarClickCallback.run());

        View closeButton = mToolbarView.findViewById(R.id.close);
        closeButton.setOnClickListener(view -> mCloseButtonCallback.run());

        mFaviconView = mToolbarView.findViewById(R.id.favicon);
        mCurrentFavicon = mFaviconView.getDrawable();

        mOnToolbarCreatedCallback.onResult(mToolbarView);
        final ViewTreeObserver observer = mToolbarView.getViewTreeObserver();
        observer.addOnPreDrawListener(
                new ViewTreeObserver.OnPreDrawListener() {
                    @Override
                    public boolean onPreDraw() {
                        // Once the toolbar layout is completed, reflect the change in height
                        // to the content view.
                        mToolbarView.getViewTreeObserver().removeOnPreDrawListener(this);
                        updateContentHeight(maxViewHeight);
                        return true;
                    }
                });
    }

    /**
     * Resizes the thin webview as per the given new max height.
     * @param maxViewHeight The maximum height of the view.
     */
    void updateContentHeight(int maxViewHeight) {
        if (maxViewHeight == 0) return;

        ViewGroup.LayoutParams layoutParams = mThinWebView.getView().getLayoutParams();
        int toolbarHeight = getToolbarHeight();
        layoutParams.height = getMaxSheetHeight(maxViewHeight) - toolbarHeight;
        mSheetContentView.setPadding(0, toolbarHeight, 0, 0);
        ViewUtils.requestLayout(mSheetContentView, "EphemeralTabSheetContent.updateContentHeight");
    }

    private int getToolbarHeight() {
        int shadowHeight =
                mContext.getResources().getDimensionPixelSize(R.dimen.action_bar_shadow_height);
        return mToolbarView.getHeight() - shadowHeight;
    }

    private int getMaxSheetHeight(int maxViewHeight) {
        // This sheet should never be taller than the tab height for it to function correctly.
        // We scale it by |FULL_HEIGHT_RATIO| to make the size equal to that of
        // ThinWebView and so it can leave a portion of the page below it visible.
        return (int) (maxViewHeight * FULL_HEIGHT_RATIO);
    }

    /** Method to be called to start the favicon anmiation. */
    public void startFaviconAnimation(Drawable favicon) {
        if (favicon == null) {
            mCurrentFavicon = null;
            mFaviconView.setImageDrawable(null);
            return;
        }

        // TODO(shaktisahu): Find out if there is a better way for this animation.
        Drawable presentedDrawable = favicon;
        if (mCurrentFavicon != null && !(mCurrentFavicon instanceof ChromeTransitionDrawable)) {
            ChromeTransitionDrawable transitionDrawable =
                    new ChromeTransitionDrawable(mCurrentFavicon, favicon);
            transitionDrawable.setCrossFadeEnabled(true);
            transitionDrawable.startTransition().setDuration(BASE_ANIMATION_DURATION_MS);
            presentedDrawable = transitionDrawable;
        }

        mFaviconView.setImageDrawable(presentedDrawable);
        mCurrentFavicon = favicon;
    }

    /** Sets the ephemeral tab title text. */
    public void updateTitle(String title) {
        TextView toolbarText = mToolbarView.findViewById(R.id.title);
        toolbarText.setText(title);
    }

    /** Sets the ephemeral tab URL. */
    public void updateURL(GURL url) {
        TextView originView = mToolbarView.findViewById(R.id.origin);
        originView.setText(
                UrlFormatter.formatUrlForSecurityDisplay(url, SchemeDisplay.OMIT_HTTP_AND_HTTPS));
    }

    /** Sets the security icon. */
    public void setSecurityIcon(@DrawableRes int resId) {
        ImageView securityIcon = mToolbarView.findViewById(R.id.security_icon);
        securityIcon.setImageResource(resId);
    }

    /** Sets the progress on the progress bar. */
    public void setProgress(float progress) {
        ProgressBar progressBar = mToolbarView.findViewById(R.id.progress_bar);
        progressBar.setProgress(Math.round(progress * 100));
    }

    /** Called to show or hide the progress bar. */
    public void setProgressVisible(boolean visible) {
        ProgressBar progressBar = mToolbarView.findViewById(R.id.progress_bar);
        progressBar.setVisibility(visible ? View.VISIBLE : View.GONE);
    }

    /**
     * Called to show (with alpha) or hide the open in new tab button.
     * @param fraction Alpha for the button when visible.
     */
    public void showOpenInNewTabButton(float fraction) {
        View button = mToolbarView.findViewById(R.id.open_in_new_tab);
        // Start showing the button about halfway toward the full state.
        if (fraction <= 0.5f) {
            if (button.getVisibility() != View.GONE) button.setVisibility(View.GONE);
        } else {
            if (button.getVisibility() != View.VISIBLE) button.setVisibility(View.VISIBLE);
            button.setAlpha((fraction - 0.5f) * 2.0f);
        }
    }

    @Override
    public View getContentView() {
        return mSheetContentView;
    }

    @Override
    public Integer getBackgroundColor() {
        return null;
    }

    @Nullable
    @Override
    public View getToolbarView() {
        return mToolbarView;
    }

    @Override
    public int getVerticalScrollOffset() {
        return mWebContents == null
                ? 0
                : RenderCoordinates.fromWebContents(mWebContents).getScrollYPixInt();
    }

    @Override
    public void destroy() {
        mThinWebView.destroy();
        mShareDelegateSupplier.destroy();
    }

    @Override
    public int getPriority() {
        return BottomSheetContent.ContentPriority.HIGH;
    }

    @Override
    public boolean swipeToDismissEnabled() {
        return true;
    }

    @Override
    public int getPeekHeight() {
        return HeightMode.DISABLED;
    }

    @Override
    public float getHalfHeightRatio() {
        return HeightMode.DEFAULT;
    }

    @Override
    public float getFullHeightRatio() {
        return HeightMode.WRAP_CONTENT;
    }

    @Override
    public boolean handleBackPress() {
        mCloseButtonCallback.run();
        return true;
    }

    @Override
    public ObservableSupplierImpl<Boolean> getBackPressStateChangedSupplier() {
        return mBackPressStateChangedSupplier;
    }

    @Override
    public void onBackPressed() {
        mCloseButtonCallback.run();
    }

    @Override
    public int getSheetContentDescriptionStringId() {
        return R.string.ephemeral_tab_sheet_description;
    }

    @Override
    public int getSheetHalfHeightAccessibilityStringId() {
        return R.string.ephemeral_tab_sheet_opened_half;
    }

    @Override
    public int getSheetFullHeightAccessibilityStringId() {
        return R.string.ephemeral_tab_sheet_opened_full;
    }

    @Override
    public int getSheetClosedAccessibilityStringId() {
        return R.string.ephemeral_tab_sheet_closed;
    }
}
