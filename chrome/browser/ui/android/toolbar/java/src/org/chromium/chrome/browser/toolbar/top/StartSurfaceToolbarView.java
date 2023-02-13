// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.top;

import android.content.Context;
import android.content.res.ColorStateList;
import android.graphics.drawable.Drawable;
import android.util.AttributeSet;
import android.view.View;
import android.view.ViewStub;
import android.widget.ImageButton;
import android.widget.LinearLayout;
import android.widget.RelativeLayout;

import androidx.annotation.Nullable;
import androidx.appcompat.content.res.AppCompatResources;

import org.chromium.chrome.browser.tabmodel.IncognitoStateProvider;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.toolbar.IncognitoToggleTabLayout;
import org.chromium.chrome.browser.toolbar.NewTabButton;
import org.chromium.chrome.browser.toolbar.R;
import org.chromium.chrome.browser.toolbar.TabCountProvider;
import org.chromium.components.browser_ui.styles.ChromeColors;
import org.chromium.components.browser_ui.widget.highlight.ViewHighlighter;
import org.chromium.components.browser_ui.widget.highlight.ViewHighlighter.HighlightParams;
import org.chromium.components.browser_ui.widget.highlight.ViewHighlighter.HighlightShape;

/** View of the StartSurfaceToolbar */
class StartSurfaceToolbarView extends RelativeLayout {
    private LinearLayout mNewTabViewWithText;
    private NewTabButton mNewTabButton;
    private boolean mShouldShowNewTabViewText;
    private View mTabSwitcherButtonView;

    @Nullable
    private IncognitoToggleTabLayout mIncognitoToggleTabLayout;
    @Nullable
    private ImageButton mIdentityDiscButton;
    private ColorStateList mLightIconTint;
    private ColorStateList mDarkIconTint;

    private boolean mIsShowing;

    private boolean mIsNewTabViewVisible;

    public StartSurfaceToolbarView(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();
        mNewTabViewWithText = findViewById(R.id.new_tab_view);
        mNewTabButton = findViewById(R.id.new_tab_button);
        ViewStub incognitoToggleTabsStub = findViewById(R.id.incognito_tabs_stub);
        mIncognitoToggleTabLayout = (IncognitoToggleTabLayout) incognitoToggleTabsStub.inflate();
        mIdentityDiscButton = findViewById(R.id.identity_disc_button);
        mTabSwitcherButtonView = findViewById(R.id.start_tab_switcher_button);
        updatePrimaryColorAndTint(false);
        mNewTabButton.setStartSurfaceEnabled(true);
    }

    void setGridTabSwitcherEnabled(boolean isGridTabSwitcherEnabled) {
        mNewTabButton.setGridTabSwitcherEnabled(isGridTabSwitcherEnabled);
    }

    /**
     * Sets whether the "+" new tab button view and "New tab" text is enabled or not.
     *
     * @param enabled The desired interactability state for the new tab button and text view.
     */
    void setNewTabEnabled(boolean enabled) {
        mNewTabButton.setEnabled(enabled);
        if (mNewTabViewWithText.getVisibility() == VISIBLE) {
            mNewTabViewWithText.setEnabled(enabled);
        }
    }

    /**
     * Sets the {@link OnClickListener} that will be notified when the New Tab view or New Tab
     * button is pressed.
     * @param listener The callback that will be notified when the New Tab view is pressed.
     */
    void setOnNewTabClickHandler(View.OnClickListener listener) {
        mNewTabViewWithText.setOnClickListener(listener);
        mNewTabButton.setOnClickListener(listener);
    }

    /**
     * @param isVisible Whether the menu button is visible.
     */
    void setMenuButtonVisibility(boolean isVisible) {
        final int buttonPaddingRight =
                (isVisible ? 0
                           : getContext().getResources().getDimensionPixelOffset(
                                   R.dimen.start_surface_toolbar_button_padding_to_edge));
        mIdentityDiscButton.setPadding(0, 0, buttonPaddingRight, 0);
    }

    /**
     * @param isVisible Whether the new tab view is visible.
     */
    void setNewTabViewVisibility(boolean isVisible) {
        mIsNewTabViewVisible = isVisible;
        updateNewTabButtonVisibility();
    }

    /**
     * Set the visibility of new tab view text.
     * @param isVisible Whether the new tab view text is visible.
     */
    void setNewTabViewTextVisibility(boolean isVisible) {
        mShouldShowNewTabViewText = isVisible;
        updateNewTabButtonVisibility();
    }

    /**
     * @param isClickable Whether the buttons are clickable.
     */
    void setButtonClickableState(boolean isClickable) {
        mNewTabViewWithText.setClickable(isClickable);
        mIncognitoToggleTabLayout.setClickable(isClickable);
    }

    /**
     * @param isVisible Whether the Incognito toggle tab layout is visible.
     */
    void setIncognitoToggleTabVisibility(boolean isVisible) {
        mIncognitoToggleTabLayout.setVisibility(getVisibility(isVisible));
    }

    /**
     * @param highlight If the new tab button should be highlighted.
     */
    void setNewTabButtonHighlight(boolean highlight) {
        if (mNewTabButton == null) return;
        if (highlight) {
            ViewHighlighter.turnOnHighlight(
                    mNewTabButton, new HighlightParams(HighlightShape.CIRCLE));
        } else {
            ViewHighlighter.turnOffHighlight(mNewTabButton);
        }
    }

    /** Called when incognito mode changes. */
    void updateIncognito(boolean isIncognito) {
        updatePrimaryColorAndTint(isIncognito);
    }

    /**
     * @param provider The {@link IncognitoStateProvider} passed to buttons that need access to it.
     */
    void setIncognitoStateProvider(IncognitoStateProvider provider) {
        mNewTabButton.setIncognitoStateProvider(provider);
    }

    /** Called when accessibility status changes. */
    void onAccessibilityStatusChanged(boolean enabled) {
        mNewTabButton.onAccessibilityStatusChanged();
    }

    /** @return The View for the identity disc. */
    View getIdentityDiscView() {
        return mIdentityDiscButton;
    }

    /**
     * @param isAtStart Whether the identity disc is at start.
     */
    void setIdentityDiscAtStart(boolean isAtStart) {
        ((LayoutParams) mIdentityDiscButton.getLayoutParams()).removeRule(RelativeLayout.START_OF);
    }

    /**
     * @param isVisible Whether the identity disc is visible.
     */
    void setIdentityDiscVisibility(boolean isVisible) {
        mIdentityDiscButton.setVisibility(getVisibility(isVisible));
    }

    /**
     * Sets the {@link OnClickListener} that will be notified when the identity disc button is
     * pressed.
     * @param listener The callback that will be notified when the identity disc  is pressed.
     */
    void setIdentityDiscClickHandler(View.OnClickListener listener) {
        mIdentityDiscButton.setOnClickListener(listener);
    }

    /**
     * Updates the image displayed on the identity disc button.
     * @param image The new image for the button.
     */
    void setIdentityDiscImage(Drawable image) {
        mIdentityDiscButton.setImageDrawable(image);
    }

    /**
     * Updates identity disc content description.
     * @param contentDescription The new description for the button.
     */
    void setIdentityDiscContentDescription(String contentDescription) {
        mIdentityDiscButton.setContentDescription(contentDescription);
    }

    /**
     * Show or hide toolbar.
     * @param shouldShowStartSurfaceToolbar Whether or not toolbar should be shown or hidden.
     * */
    void setToolbarVisibility(boolean shouldShowStartSurfaceToolbar) {
        if (shouldShowStartSurfaceToolbar == mIsShowing) return;
        mIsShowing = shouldShowStartSurfaceToolbar;

        // TODO(https://crbug.com/1139024): The fade animator of toolbar view should always show by
        // default. Not showing fade animator is a temporary solution for crbug.com/1249377.
        setVisibility(getVisibility(shouldShowStartSurfaceToolbar));
    }

    /**
     * @param isVisible Whether the tab switcher button is visible.
     */
    void setTabSwitcherButtonVisibility(boolean isVisible) {
        mTabSwitcherButtonView.setVisibility(getVisibility(isVisible));
    }

    /**
     * Set TabCountProvider for incognito toggle view.
     * @param tabCountProvider The {@link TabCountProvider} to update the incognito toggle view.
     */
    void setTabCountProvider(TabCountProvider tabCountProvider) {
        if (mIncognitoToggleTabLayout != null) {
            mIncognitoToggleTabLayout.setTabCountProvider(tabCountProvider);
        }
    }

    /**
     * Set TabModelSelector for incognito toggle view.
     * @param selector  A {@link TabModelSelector} to provide information about open tabs.
     */
    void setTabModelSelector(TabModelSelector selector) {
        if (mIncognitoToggleTabLayout != null) {
            mIncognitoToggleTabLayout.setTabModelSelector(selector);
        }
    }

    private void updateNewTabButtonVisibility() {
        if (!mIsNewTabViewVisible) {
            mNewTabButton.setVisibility(GONE);
            mNewTabViewWithText.setVisibility(GONE);
            return;
        }

        mNewTabButton.setVisibility(mShouldShowNewTabViewText ? GONE : VISIBLE);
        mNewTabViewWithText.setVisibility(!mShouldShowNewTabViewText ? GONE : VISIBLE);

        // This is a workaround for the issue that the UrlBar is given the default focus on
        // Android versions before Pie when showing the start surface toolbar with the new
        // tab button (UrlBar is invisible to users). Check crbug.com/1081538 for more
        // details.
        if (mShouldShowNewTabViewText) {
            mNewTabViewWithText.getParent().requestChildFocus(
                    mNewTabViewWithText, mNewTabViewWithText);
        } else {
            mNewTabViewWithText.getParent().requestChildFocus(mNewTabButton, mNewTabButton);
        }
    }

    private void updatePrimaryColorAndTint(boolean isIncognito) {
        int primaryColor = ChromeColors.getPrimaryBackgroundColor(getContext(), isIncognito);
        setBackgroundColor(primaryColor);

        if (mLightIconTint == null) {
            mLightIconTint = AppCompatResources.getColorStateList(
                    getContext(), R.color.default_icon_color_light_tint_list);
            mDarkIconTint = AppCompatResources.getColorStateList(
                    getContext(), R.color.default_icon_color_tint_list);
        }
    }

    private int getVisibility(boolean isVisible) {
        return isVisible ? View.VISIBLE : View.GONE;
    }
}
