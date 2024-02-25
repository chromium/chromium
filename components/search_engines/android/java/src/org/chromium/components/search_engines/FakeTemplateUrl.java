// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.search_engines;

import android.text.TextUtils;

public class FakeTemplateUrl extends TemplateUrl {
    private static int sPrepopulatedInstanceCount;
    private final String mShortName;
    private final String mKeyword;
    private final int mPrepopulatedId;

    public FakeTemplateUrl(String shortName, String keyword) {
        super(0);
        mShortName = shortName;
        mKeyword = keyword;
        mPrepopulatedId = ++sPrepopulatedInstanceCount;
    }

    @Override
    public String getShortName() {
        return mShortName;
    }

    @Override
    public String getKeyword() {
        return mKeyword;
    }

    @Override
    public int getPrepopulatedId() {
        return mPrepopulatedId;
    }

    @Override
    public boolean getIsPrepopulated() {
        return mPrepopulatedId == 0;
    }

    @Override
    public boolean equals(Object other) {
        if (!(other instanceof FakeTemplateUrl)) return false;
        FakeTemplateUrl otherTemplateUrl = (FakeTemplateUrl) other;
        return mPrepopulatedId == otherTemplateUrl.mPrepopulatedId
                && TextUtils.equals(mKeyword, otherTemplateUrl.mKeyword)
                && TextUtils.equals(mShortName, otherTemplateUrl.mShortName);
    }
}
