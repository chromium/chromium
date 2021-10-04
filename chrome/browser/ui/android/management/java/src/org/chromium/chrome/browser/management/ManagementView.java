// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.management;

import android.content.Context;
import android.text.Html;
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

    private static final String LEARN_MORE_URL =
            "https://support.google.com/chrome/answer/9281740?p=is_chrome_managed&visit_id=637678488620233541-4078225067&rd=1";

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

        // Enable learn more link.
        String learnMoreText =
                getResources().getString(R.string.management_learn_more, LEARN_MORE_URL);

        mLearnMore.setText(Html.fromHtml(learnMoreText));
        mLearnMore.setMovementMethod(LinkMovementMethod.getInstance());

        // Set default management status
        mIsManaged = false;
        mManagerName = null;
        adjustView();
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
