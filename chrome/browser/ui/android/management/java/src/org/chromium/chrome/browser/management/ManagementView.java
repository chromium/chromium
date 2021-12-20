// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.management;

import android.content.Context;
import android.text.SpannableString;
import android.text.TextUtils;
import android.text.method.LinkMovementMethod;
import android.util.AttributeSet;
import android.widget.LinearLayout;
import android.widget.TextView;

import androidx.annotation.Nullable;

/**
 * The View that renders the ManagementPage (chrome://management).
 * Consists of an medium size image icon over title and descriptive text.
 */
public class ManagementView extends LinearLayout {
    private boolean mIsManaged;
    private @Nullable String mManagerName;

    private TextView mTitle;
    private TextView mDescription;
    private TextView mLearnMore;

    /** Constructor for inflating from XML. */
    public ManagementView(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    @Override
    public void onFinishInflate() {
        super.onFinishInflate();

        mTitle = (TextView) findViewById(R.id.title_text);
        mDescription = (TextView) findViewById(R.id.description_text);
        mLearnMore = (TextView) findViewById(R.id.learn_more);

        // Set default management status
        mIsManaged = false;
        mManagerName = null;
        adjustView();

        // Making the view focusable ensures that it will be presented to the user once they select
        // the page on the Omnibox. When the view is not focusable, the keyboard needs to be
        // dismissed before the page is shown.
        setFocusable(true);
        setFocusableInTouchMode(true);
    }

    /** Sets whether account is managed. Then updates view accordingly. */
    public void setManaged(boolean isManaged) {
        if (mIsManaged != isManaged) {
            mIsManaged = isManaged;
            adjustView();
        }
    }

    /** Gets whether account is managed. */
    public boolean isManaged() {
        return mIsManaged;
    }

    /** Sets account manager name. Then updates view accordingly.  */
    public void setManagerName(@Nullable String managerName) {
        if (!TextUtils.equals(mManagerName, managerName)) {
            mManagerName = managerName;
            adjustView();
        }
    }

    /** Gets account manager name. */
    public @Nullable String getManagerName() {
        return mManagerName;
    }

    public void setLearnMoreText(SpannableString learnMoreText) {
        mLearnMore.setText(learnMoreText);
        mLearnMore.setMovementMethod(LinkMovementMethod.getInstance());
    }

    /**
     * Adjusts Title, Description, and Learn More link based on management status.
     */
    private void adjustView() {
        if (mIsManaged) {
            if (TextUtils.isEmpty(mManagerName)) {
                mTitle.setText(getResources().getString(R.string.management_subtitle));
            } else {
                mTitle.setText(getResources().getString(
                        R.string.management_subtitle_managed_by, mManagerName));
            }
        } else {
            mTitle.setText(getResources().getString(R.string.management_not_managed_subtitle));
        }

        mDescription.setVisibility(mIsManaged ? VISIBLE : INVISIBLE);
        mLearnMore.setVisibility(mIsManaged ? VISIBLE : INVISIBLE);
    }
}
