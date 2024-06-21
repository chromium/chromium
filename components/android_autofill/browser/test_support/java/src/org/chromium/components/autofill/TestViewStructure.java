// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.autofill;

import android.graphics.Matrix;
import android.graphics.Rect;
import android.os.Bundle;
import android.os.LocaleList;
import android.os.Parcel;
import android.util.Pair;
import android.view.View;
import android.view.ViewStructure;
import android.view.autofill.AutofillId;
import android.view.autofill.AutofillValue;

import java.util.ArrayList;
import java.util.List;

/** This class only implements the necessary methods of ViewStructure for testing. */
public class TestViewStructure extends ViewStructure {
    /** Test implementation of {@link HtmlInfo}. */
    public static class TestHtmlInfo extends HtmlInfo {
        private String mTag;
        private List<Pair<String, String>> mAttributes;

        public TestHtmlInfo(String tag, List<Pair<String, String>> attributes) {
            mTag = tag;
            mAttributes = attributes;
        }

        @Override
        public List<Pair<String, String>> getAttributes() {
            return mAttributes;
        }

        public String getAttribute(String attribute) {
            for (Pair<String, String> pair : mAttributes) {
                if (attribute.equals(pair.first)) {
                    return pair.second;
                }
            }
            return null;
        }

        @Override
        public String getTag() {
            return mTag;
        }
    }

    /** Test implementation of {@link HtmlInfo.Builder}. */
    public static class TestBuilder extends HtmlInfo.Builder {
        private String mTag;
        private ArrayList<Pair<String, String>> mAttributes;

        public TestBuilder(String tag) {
            mTag = tag;
            mAttributes = new ArrayList<Pair<String, String>>();
        }

        @Override
        public HtmlInfo.Builder addAttribute(String name, String value) {
            mAttributes.add(new Pair<String, String>(name, value));
            return this;
        }

        @Override
        public HtmlInfo build() {
            return new TestHtmlInfo(mTag, mAttributes);
        }
    }

    public TestViewStructure() {
        mChildren = new ArrayList<TestViewStructure>();
    }

    @Override
    public void setAlpha(float alpha) {}

    @Override
    public void setAccessibilityFocused(boolean state) {}

    @Override
    public void setCheckable(boolean state) {}

    @Override
    public void setChecked(boolean state) {
        mChecked = state;
    }

    public boolean getChecked() {
        return mChecked;
    }

    @Override
    public void setActivated(boolean state) {}

    @Override
    public CharSequence getText() {
        return null;
    }

    @Override
    public int getTextSelectionStart() {
        return 0;
    }

    @Override
    public int getTextSelectionEnd() {
        return 0;
    }

    @Override
    public CharSequence getHint() {
        return mHint;
    }

    @Override
    public Bundle getExtras() {
        if (mBundle == null) mBundle = new Bundle();
        return mBundle;
    }

    @Override
    public boolean hasExtras() {
        return mBundle != null;
    }

    @Override
    public void setChildCount(int num) {}

    @Override
    public int addChildCount(int num) {
        int index = mChildCount;
        mChildCount += num;
        mChildren.ensureCapacity(mChildCount);
        return index;
    }

    @Override
    public int getChildCount() {
        return mChildren.size();
    }

    @Override
    public ViewStructure newChild(int index) {
        TestViewStructure viewStructure = new TestViewStructure();
        if (index < mChildren.size()) {
            mChildren.set(index, viewStructure);
        } else if (index == mChildren.size()) {
            mChildren.add(viewStructure);
        } else {
            // Failed intentionally, we shouldn't run into this case.
            mChildren.add(index, viewStructure);
        }
        return viewStructure;
    }

    public TestViewStructure getChild(int index) {
        return mChildren.get(index);
    }

    @Override
    public ViewStructure asyncNewChild(int index) {
        return null;
    }

    @Override
    public void asyncCommit() {}

    @Override
    public AutofillId getAutofillId() {
        // Check AutofillId.java for more information:
        // https://cs.android.com/android/platform/superproject/+/master:frameworks/base/core/java/android/view/autofill/AutofillId.java
        Parcel parcel = Parcel.obtain();
        // The host View - set to 0 by default.
        parcel.writeInt(0);
        // Corresponds to FLAG_IS_VIRTUAL_INT and indicates that the virtual id
        // that comes next is an int.
        parcel.writeInt(1);
        // The actual virtual id.
        parcel.writeInt(mId);
        parcel.setDataPosition(0);

        return AutofillId.CREATOR.createFromParcel(parcel);
    }

    @Override
    public HtmlInfo.Builder newHtmlInfoBuilder(String tag) {
        return new TestBuilder(tag);
    }

    @Override
    public void setAutofillHints(String[] arg0) {
        mAutofillHints = arg0.clone();
    }

    public String[] getAutofillHints() {
        if (mAutofillHints == null) return null;
        return mAutofillHints.clone();
    }

    @Override
    public void setAutofillId(AutofillId arg0) {}

    @Override
    public void setAutofillId(AutofillId arg0, int arg1) {
        mId = arg1;
    }

    public int getId() {
        return mId;
    }

    @Override
    public void setAutofillOptions(CharSequence[] arg0) {
        mAutofillOptions = arg0.clone();
    }

    public CharSequence[] getAutofillOptions() {
        if (mAutofillOptions == null) return null;
        return mAutofillOptions.clone();
    }

    @Override
    public void setAutofillType(int arg0) {
        mAutofillType = arg0;
    }

    public int getAutofillType() {
        return mAutofillType;
    }

    @Override
    public void setAutofillValue(AutofillValue arg0) {
        mAutofillValue = arg0;
    }

    public AutofillValue getAutofillValue() {
        return mAutofillValue;
    }

    @Override
    public void setId(int id, String packageName, String typeName, String entryName) {}

    @Override
    public void setDimens(int left, int top, int scrollX, int scrollY, int width, int height) {
        mDimensRect = new Rect(left, top, width + left, height + top);
        mDimensScrollX = scrollX;
        mDimensScrollY = scrollY;
    }

    public Rect getDimensRect() {
        return mDimensRect;
    }

    public int getDimensScrollX() {
        return mDimensScrollX;
    }

    public int getDimensScrollY() {
        return mDimensScrollY;
    }

    @Override
    public void setElevation(float elevation) {}

    @Override
    public void setEnabled(boolean state) {}

    @Override
    public void setClickable(boolean state) {}

    @Override
    public void setLongClickable(boolean state) {}

    @Override
    public void setContextClickable(boolean state) {}

    @Override
    public void setFocusable(boolean state) {}

    @Override
    public void setFocused(boolean state) {
        mFocused = state;
    }

    public boolean getFocused() {
        return mFocused;
    }

    @Override
    public void setClassName(String className) {
        mClassName = className;
    }

    public String getClassName() {
        return mClassName;
    }

    @Override
    public void setContentDescription(CharSequence contentDescription) {}

    @Override
    public void setDataIsSensitive(boolean arg0) {
        mDataIsSensitive = arg0;
    }

    public boolean getDataIsSensitive() {
        return mDataIsSensitive;
    }

    @Override
    public void setHint(CharSequence hint) {
        mHint = hint;
    }

    @Override
    public void setHtmlInfo(HtmlInfo arg0) {
        mHtmlInfo = (TestHtmlInfo) arg0;
    }

    public TestHtmlInfo getHtmlInfo() {
        return mHtmlInfo;
    }

    @Override
    public void setInputType(int arg0) {}

    @Override
    public void setLocaleList(LocaleList arg0) {}

    @Override
    public void setOpaque(boolean arg0) {}

    @Override
    public void setTransformation(Matrix matrix) {}

    @Override
    public void setVisibility(int visibility) {
        mVisibility = visibility;
    }

    public int getVisibility() {
        return mVisibility;
    }

    @Override
    public void setSelected(boolean state) {}

    @Override
    public void setText(CharSequence text) {}

    @Override
    public void setText(CharSequence text, int selectionStart, int selectionEnd) {}

    @Override
    public void setTextStyle(float size, int fgColor, int bgColor, int style) {}

    @Override
    public void setTextLines(int[] charOffsets, int[] baselines) {}

    @Override
    public void setWebDomain(String webDomain) {
        mWebDomain = webDomain;
    }

    public String getWebDomain() {
        return mWebDomain;
    }

    private int mAutofillType;
    private CharSequence mHint;
    private String[] mAutofillHints;
    private int mId;
    private boolean mFocused;
    private String mClassName;
    private String mWebDomain;
    private int mChildCount;
    private ArrayList<TestViewStructure> mChildren;
    private CharSequence[] mAutofillOptions;
    private AutofillValue mAutofillValue;
    private boolean mDataIsSensitive;
    private TestHtmlInfo mHtmlInfo;
    private boolean mChecked;
    private Rect mDimensRect;
    private int mDimensScrollX;
    private int mDimensScrollY;
    private Bundle mBundle;
    // Initializes to the value AutofillProvider will never use.
    private int mVisibility = View.GONE;
}
