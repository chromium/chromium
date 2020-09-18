// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.page_info;

import android.animation.Animator;
import android.animation.AnimatorSet;
import android.animation.ObjectAnimator;
import android.content.Context;
import android.text.Layout;
import android.util.AttributeSet;
import android.view.LayoutInflater;
import android.view.View;
import android.view.View.OnClickListener;
import android.widget.Button;
import android.widget.FrameLayout;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.TextView;

import androidx.annotation.ColorRes;
import androidx.annotation.DrawableRes;
import androidx.annotation.StringRes;
import androidx.annotation.VisibleForTesting;
import androidx.appcompat.widget.AppCompatTextView;

import org.chromium.ui.UiUtils;

import java.util.ArrayList;
import java.util.List;

/**
 * Represents the view inside the page info popup.
 */
public class PageInfoView extends FrameLayout implements OnClickListener {
    /**
     * A TextView which truncates and displays a URL such that the origin is always visible.
     * The URL can be expanded by clicking on the it.
     */
    public static class ElidedUrlTextView extends AppCompatTextView {
        // The number of lines to display when the URL is truncated. This number
        // should still allow the origin to be displayed. NULL before
        // setUrlAfterLayout() is called.
        private Integer mTruncatedUrlLinesToDisplay;

        // The number of lines to display when the URL is expanded. This should be enough to display
        // at most two lines of the fragment if there is one in the URL.
        private Integer mFullLinesToDisplay;

        // If true, the text view will show the truncated text. If false, it
        // will show the full, expanded text.
        private boolean mIsShowingTruncatedText = true;

        // The length of the URL's origin in number of characters.
        private int mOriginLength = -1;

        // The maximum number of lines currently shown in the view
        private int mCurrentMaxLines = Integer.MAX_VALUE;

        /** Constructor for inflating from XML. */
        public ElidedUrlTextView(Context context, AttributeSet attrs) {
            super(context, attrs);
        }

        @Override
        public void setMaxLines(int maxlines) {
            super.setMaxLines(maxlines);
            mCurrentMaxLines = maxlines;
        }

        /**
         * Find the number of lines of text which must be shown in order to display the character at
         * a given index.
         */
        private int getLineForIndex(int index) {
            Layout layout = getLayout();
            int endLine = 0;
            while (endLine < layout.getLineCount() && layout.getLineEnd(endLine) < index) {
                endLine++;
            }
            // Since endLine is an index, add 1 to get the number of lines.
            return endLine + 1;
        }

        @Override
        protected void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
            setMaxLines(Integer.MAX_VALUE);
            super.onMeasure(widthMeasureSpec, heightMeasureSpec);
            assert mOriginLength >= 0 : "setUrl() must be called before layout.";
            String urlText = getText().toString();

            // Find the range of lines containing the origin.
            int originEndLine = getLineForIndex(mOriginLength);

            // Display an extra line so we don't accidentally hide the origin with
            // ellipses
            mTruncatedUrlLinesToDisplay = originEndLine + 1;

            // Find the line where the fragment starts. Since # is a reserved character, it is safe
            // to just search for the first # to appear in the url.
            int fragmentStartIndex = urlText.indexOf('#');
            if (fragmentStartIndex == -1) fragmentStartIndex = urlText.length();

            int fragmentStartLine = getLineForIndex(fragmentStartIndex);
            mFullLinesToDisplay = fragmentStartLine + 1;

            // If there is no origin (according to OmniboxUrlEmphasizer), make sure the fragment is
            // still hidden correctly.
            if (mFullLinesToDisplay < mTruncatedUrlLinesToDisplay) {
                mTruncatedUrlLinesToDisplay = mFullLinesToDisplay;
            }

            if (updateMaxLines()) super.onMeasure(widthMeasureSpec, heightMeasureSpec);
        }

        /**
         * Sets the URL and the length of the URL's origin.
         * Must be called before layout.
         *
         * @param url The URL.
         * @param originLength The length of the URL's origin in number of characters.
         */
        public void setUrl(CharSequence url, int originLength) {
            assert originLength >= 0 && originLength <= url.length();
            setText(url);
            mOriginLength = originLength;
        }

        /**
         * Toggles truncating/expanding the URL text. If the URL text is not
         * truncated, has no effect.
         */
        public void toggleTruncation() {
            mIsShowingTruncatedText = !mIsShowingTruncatedText;
            if (mFullLinesToDisplay != null) {
                updateMaxLines();
            }
        }

        private boolean updateMaxLines() {
            int maxLines = mFullLinesToDisplay;
            if (mIsShowingTruncatedText) {
                maxLines = mTruncatedUrlLinesToDisplay;
            }
            if (maxLines != mCurrentMaxLines) {
                setMaxLines(maxLines);
                return true;
            }
            return false;
        }
    }

    /**  Parameters to configure the view of the page info popup. */
    public static class PageInfoViewParams {
        public boolean urlTitleShown = true;
        public boolean connectionMessageShown = true;
        public boolean instantAppButtonShown = true;
        public boolean siteSettingsButtonShown = true;
        public boolean openOnlineButtonShown = true;
        public boolean previewUIShown = true;
        public boolean previewSeparatorShown = true;
        public boolean cookieControlsShown = true;

        public Runnable urlTitleClickCallback;
        public Runnable urlTitleLongClickCallback;
        public Runnable instantAppButtonClickCallback;
        public Runnable siteSettingsButtonClickCallback;
        public Runnable openOnlineButtonClickCallback;
        public Runnable previewShowOriginalClickCallback;
        public Runnable onUiClosingCallback;

        public CharSequence url;
        public CharSequence previewLoadOriginalMessage;
        public int urlOriginLength;
    }

    /** Parameters to configure the permission info section */
    public static class PermissionParams {
        public boolean show_title = true;
        public List<PermissionRowParams> permissions;
    }

    /**  Parameters to configure the view of a permission row. */
    public static class PermissionRowParams {
        public CharSequence name;
        public boolean allowed;
        // TODO(crbug.com/1077766): Remove status text and associations after migration.
        public CharSequence status;
        public @DrawableRes int iconResource;
        public @ColorRes int iconTintColorResource;
        public @StringRes int warningTextResource;
        public @StringRes int subtitleTextResource;
        public Runnable clickCallback;
    }

    /**  Parameters to configure the view of the connection message. */
    public static class ConnectionInfoParams {
        public CharSequence message;
        public CharSequence summary;
        public Runnable clickCallback;
    }

    protected static final int FADE_DURATION_MS = 200;
    protected static final int FADE_IN_BASE_DELAY_MS = 150;
    protected static final int FADE_IN_DELAY_OFFSET_MS = 20;

    // Shared UI components between PageInfoView and PageInfoViewV2. This list should shrink as
    // these components are replaced with different UI and eventually this class will be replaced
    // completely.
    protected ElidedUrlTextView mUrlTitle;
    protected TextView mPreviewMessage;
    protected TextView mPreviewLoadOriginal;
    protected View mPreviewSeparator;
    protected Button mInstantAppButton;
    protected Button mSiteSettingsButton;
    protected Button mOpenOnlineButton;
    protected Runnable mOnUiClosingCallback;

    // Components specific to this PageInfoView
    private TextView mConnectionSummary;
    private TextView mConnectionMessage;
    private TextView mPerformanceSummary;
    private TextView mPerformanceMessage;
    private TextView mPermissionsTitle;
    private View mPermissionsSeparator;
    private LinearLayout mPermissionsList;
    private TextView mHttpsImageCompressionMessage;
    private View mCookieControlsSeparator;
    private CookieControlsView mCookieControlsView;

    public PageInfoView(Context context) {
        super(context);
    }

    public PageInfoView(Context context, PageInfoViewParams params) {
        super(context);
        LayoutInflater.from(context).inflate(R.layout.page_info, this, true);
        init(params);
    }

    protected void init(PageInfoViewParams params) {
        initUrlTitle(params);
        initPreview(params);
        initConnection(params);
        initPerformance(params);
        initHttpsImageCompression(params);
        initPermissions(params);
        initCookies(params);
        initInstantApp(params);
        initSiteSettings(params);
        initOpenOnline(params);
    }

    protected void initUrlTitle(PageInfoViewParams params) {
        mUrlTitle = findViewById(R.id.page_info_url);
        mUrlTitle.setUrl(params.url, params.urlOriginLength);
        if (params.urlTitleLongClickCallback != null) {
            mUrlTitle.setOnLongClickListener(v -> {
                params.urlTitleLongClickCallback.run();
                return true;
            });
        }
        initializePageInfoViewChild(mUrlTitle, params.urlTitleShown, params.urlTitleClickCallback);
    }

    protected void initPreview(PageInfoViewParams params) {
        mPreviewMessage = findViewById(R.id.page_info_preview_message);
        mPreviewLoadOriginal = findViewById(R.id.page_info_preview_load_original);
        mPreviewSeparator = findViewById(R.id.page_info_preview_separator);
        initializePageInfoViewChild(mPreviewMessage, params.previewUIShown, null);
        initializePageInfoViewChild(mPreviewLoadOriginal, params.previewUIShown,
                params.previewShowOriginalClickCallback);
        initializePageInfoViewChild(mPreviewSeparator, params.previewSeparatorShown, null);
        mPreviewLoadOriginal.setText(params.previewLoadOriginalMessage);
    }

    protected void initConnection(PageInfoViewParams params) {
        mConnectionSummary = findViewById(R.id.page_info_connection_summary);
        mConnectionMessage = findViewById(R.id.page_info_connection_message);
        // Hide the connection summary until its text is set.
        initializePageInfoViewChild(mConnectionSummary, false, null);
        initializePageInfoViewChild(mConnectionMessage, params.connectionMessageShown, null);
    }

    protected void initPerformance(PageInfoViewParams params) {
        mPerformanceSummary = findViewById(R.id.page_info_performance_summary);
        mPerformanceMessage = findViewById(R.id.page_info_performance_message);
        initializePageInfoViewChild(mPerformanceSummary, false, null);
        initializePageInfoViewChild(mPerformanceMessage, false, null);
    }

    protected void initHttpsImageCompression(PageInfoViewParams params) {
        mHttpsImageCompressionMessage =
                findViewById(R.id.page_info_lite_mode_https_image_compression_message);
        initializePageInfoViewChild(mHttpsImageCompressionMessage, false, null);
    }

    protected void initPermissions(PageInfoViewParams params) {
        mPermissionsTitle = findViewById(R.id.page_info_permissions_list_title);
        mPermissionsSeparator = findViewById(R.id.page_info_permissions_separator);
        mPermissionsList = findViewById(R.id.page_info_permissions_list);
        // Hide the permissions list for sites with no permissions.
        initializePageInfoViewChild(mPermissionsTitle, false, null);
        initializePageInfoViewChild(mPermissionsSeparator, false, null);
        initializePageInfoViewChild(mPermissionsList, false, null);
    }

    protected void initCookies(PageInfoViewParams params) {
        mCookieControlsSeparator = findViewById(R.id.page_info_cookie_controls_separator);
        mCookieControlsView = findViewById(R.id.page_info_cookie_controls_view);
        initializePageInfoViewChild(mCookieControlsSeparator, params.cookieControlsShown, null);
        initializePageInfoViewChild(mCookieControlsView, params.cookieControlsShown, null);
        mOnUiClosingCallback = params.onUiClosingCallback;
    }

    protected void initInstantApp(PageInfoViewParams params) {
        mInstantAppButton = findViewById(R.id.page_info_instant_app_button);
        initializePageInfoViewChild(mInstantAppButton, params.instantAppButtonShown,
                params.instantAppButtonClickCallback);
    }

    protected void initSiteSettings(PageInfoViewParams params) {
        mSiteSettingsButton = findViewById(R.id.page_info_site_settings_button);
        initializePageInfoViewChild(mSiteSettingsButton, params.siteSettingsButtonShown,
                params.siteSettingsButtonClickCallback);
    }

    protected void initOpenOnline(PageInfoViewParams params) {
        mOpenOnlineButton = findViewById(R.id.page_info_open_online_button);
        // The open online button should not fade in.
        initializePageInfoViewChild(mOpenOnlineButton, params.openOnlineButtonShown,
                params.openOnlineButtonClickCallback);
    }

    public CookieControlsView getCookieControlsView() {
        return mCookieControlsView;
    }

    // FrameLayout:
    @Override
    protected void onDetachedFromWindow() {
        super.onDetachedFromWindow();
        mOnUiClosingCallback.run();
    }

    public void setPermissions(PermissionParams params) {
        mPermissionsList.removeAllViews();
        // If we have at least one permission show the lower permissions area.
        mPermissionsList.setVisibility(!params.permissions.isEmpty() ? View.VISIBLE : View.GONE);
        mPermissionsTitle.setVisibility(params.show_title ? View.VISIBLE : View.GONE);
        mPermissionsSeparator.setVisibility(params.show_title ? View.VISIBLE : View.GONE);
        for (PermissionRowParams rowParams : params.permissions) {
            mPermissionsList.addView(createPermissionRow(rowParams));
        }
    }

    public void setConnectionInfo(ConnectionInfoParams params) {
        if (params.summary != null) {
            mConnectionSummary.setVisibility(View.VISIBLE);
            mConnectionSummary.setText(params.summary);
        }
        if (params.message != null) {
            mConnectionMessage.setVisibility(View.VISIBLE);
            mConnectionMessage.setText(params.message);
            if (params.clickCallback != null) {
                mConnectionMessage.setTag(R.id.page_info_click_callback, params.clickCallback);
                mConnectionMessage.setOnClickListener(this);
            }
        }
    }

    public void showPerformanceInfo(boolean show) {
        if (show) {
            mPerformanceSummary.setVisibility(View.VISIBLE);
            mPerformanceMessage.setVisibility(View.VISIBLE);
        } else {
            mPerformanceSummary.setVisibility(View.GONE);
            mPerformanceMessage.setVisibility(View.GONE);
        }
    }

    public void showHttpsImageCompressionInfo(boolean show) {
        if (show) {
            mHttpsImageCompressionMessage.setVisibility(View.VISIBLE);
        } else {
            mHttpsImageCompressionMessage.setVisibility(View.GONE);
        }
    }

    public Animator createEnterExitAnimation(boolean isEnter) {
        return createFadeAnimations(isEnter);
    }

    public void toggleUrlTruncation() {
        mUrlTitle.toggleTruncation();
    }

    public void disableInstantAppButton() {
        mInstantAppButton.setEnabled(false);
    }

    @Override
    public void onClick(View view) {
        Object clickCallbackObj = view.getTag(R.id.page_info_click_callback);
        if (!(clickCallbackObj instanceof Runnable)) {
            throw new IllegalStateException("Unable to find click callback for view: " + view);
        }
        Runnable clickCallback = (Runnable) clickCallbackObj;
        clickCallback.run();
    }

    protected void initializePageInfoViewChild(View child, boolean shown, Runnable clickCallback) {
        child.setVisibility(shown ? View.VISIBLE : View.GONE);
        child.setTag(R.id.page_info_click_callback, clickCallback);
        if (clickCallback == null) return;
        child.setOnClickListener(this);
    }

    private View createPermissionRow(PermissionRowParams params) {
        View permissionRow =
                LayoutInflater.from(getContext()).inflate(R.layout.page_info_permission_row, null);

        TextView permissionStatus = permissionRow.findViewById(R.id.page_info_permission_status);
        permissionStatus.setText(params.status);

        ImageView permissionIcon = permissionRow.findViewById(R.id.page_info_permission_icon);
        permissionIcon.setImageDrawable(UiUtils.getTintedDrawable(getContext(), params.iconResource,
                params.iconTintColorResource != 0 ? params.iconTintColorResource
                                                  : R.color.default_icon_color));

        if (params.warningTextResource != 0) {
            TextView permissionUnavailable =
                    permissionRow.findViewById(R.id.page_info_permission_unavailable_message);
            permissionUnavailable.setVisibility(View.VISIBLE);
            permissionUnavailable.setText(params.warningTextResource);
        }

        if (params.subtitleTextResource != 0) {
            TextView permissionSubtitle =
                    permissionRow.findViewById(R.id.page_info_permission_subtitle);
            permissionSubtitle.setVisibility(View.VISIBLE);
            permissionSubtitle.setText(params.subtitleTextResource);
        }

        if (params.clickCallback != null) {
            permissionRow.setTag(R.id.page_info_click_callback, params.clickCallback);
            permissionRow.setOnClickListener(this);
        }

        return permissionRow;
    }

    /**
     * Create a list of all the views which we want to individually fade in.
     */
    protected List<View> collectAnimatableViews() {
        List<View> animatableViews = new ArrayList<>();
        animatableViews.add(mUrlTitle);
        animatableViews.add(mConnectionSummary);
        animatableViews.add(mConnectionMessage);
        animatableViews.add(mPerformanceSummary);
        animatableViews.add(mPerformanceMessage);
        animatableViews.add(mPreviewSeparator);
        animatableViews.add(mPreviewMessage);
        animatableViews.add(mPreviewLoadOriginal);
        animatableViews.add(mHttpsImageCompressionMessage);
        animatableViews.add(mInstantAppButton);
        animatableViews.add(mCookieControlsSeparator);
        animatableViews.add(mCookieControlsView);
        animatableViews.add(mPermissionsSeparator);
        animatableViews.add(mPermissionsTitle);
        for (int i = 0; i < mPermissionsList.getChildCount(); i++) {
            animatableViews.add(mPermissionsList.getChildAt(i));
        }
        animatableViews.add(mSiteSettingsButton);

        return animatableViews;
    }

    /**
     * Create an animator to fade an individual dialog element.
     */
    protected Animator createInnerFadeAnimation(final View view, int position, boolean isEnter) {
        ObjectAnimator alphaAnim;

        if (isEnter) {
            view.setAlpha(0f);
            alphaAnim = ObjectAnimator.ofFloat(view, View.ALPHA, 1f);
            alphaAnim.setStartDelay(FADE_IN_BASE_DELAY_MS + FADE_IN_DELAY_OFFSET_MS * position);
        } else {
            alphaAnim = ObjectAnimator.ofFloat(view, View.ALPHA, 0f);
        }

        alphaAnim.setDuration(FADE_DURATION_MS);
        return alphaAnim;
    }

    /**
     * Create animations for fading the view in/out.
     */
    protected Animator createFadeAnimations(boolean isEnter) {
        AnimatorSet animation = new AnimatorSet();
        AnimatorSet.Builder builder = animation.play(new AnimatorSet());

        List<View> animatableViews = collectAnimatableViews();
        for (int i = 0; i < animatableViews.size(); i++) {
            View view = animatableViews.get(i);
            if (view.getVisibility() == View.VISIBLE) {
                Animator anim = createInnerFadeAnimation(view, i, isEnter);
                builder.with(anim);
            }
        }

        return animation;
    }

    @VisibleForTesting
    public String getUrlTitleForTesting() {
        return mUrlTitle.getText().toString();
    }
}
