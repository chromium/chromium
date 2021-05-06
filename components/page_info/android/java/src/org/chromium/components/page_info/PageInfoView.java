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
import android.widget.TextView;

import androidx.appcompat.widget.AppCompatTextView;

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
        public boolean openOnlineButtonShown = true;
        public boolean cookieControlsShown = true;

        public Runnable urlTitleClickCallback;
        public Runnable urlTitleLongClickCallback;
        public Runnable instantAppButtonClickCallback;
        public Runnable openOnlineButtonClickCallback;
        public Runnable onUiClosingCallback;

        public CharSequence url;
        public int urlOriginLength;
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
    protected Button mInstantAppButton;
    protected Button mOpenOnlineButton;
    protected Runnable mOnUiClosingCallback;

    // Components specific to this PageInfoView
    private TextView mConnectionSummary;
    private TextView mConnectionMessage;
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
        initConnection(params);
        initHttpsImageCompression(params);
        initPermissions(params);
        initCookies(params);
        initInstantApp(params);
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

    protected void initConnection(PageInfoViewParams params) {
        mConnectionSummary = findViewById(R.id.page_info_connection_summary);
        mConnectionMessage = findViewById(R.id.page_info_connection_message);
        // Hide the connection summary until its text is set.
        initializePageInfoViewChild(mConnectionSummary, false, null);
        initializePageInfoViewChild(mConnectionMessage, params.connectionMessageShown, null);
    }

    protected void initHttpsImageCompression(PageInfoViewParams params) {
        mHttpsImageCompressionMessage =
                findViewById(R.id.page_info_lite_mode_https_image_compression_message);
        initializePageInfoViewChild(mHttpsImageCompressionMessage, false, null);
    }

    protected void initPermissions(PageInfoViewParams params) {
        // TODO(crbug.com/1182193): Remove function and restructure init at the end of cleanup.
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

    /**
     * Create a list of all the views which we want to individually fade in.
     */
    protected List<View> collectAnimatableViews() {
        List<View> animatableViews = new ArrayList<>();
        animatableViews.add(mUrlTitle);
        animatableViews.add(mConnectionSummary);
        animatableViews.add(mConnectionMessage);
        animatableViews.add(mHttpsImageCompressionMessage);
        animatableViews.add(mInstantAppButton);
        animatableViews.add(mCookieControlsSeparator);
        animatableViews.add(mCookieControlsView);

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
}
