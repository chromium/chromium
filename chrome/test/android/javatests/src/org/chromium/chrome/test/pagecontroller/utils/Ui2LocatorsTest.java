// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.pagecontroller.utils;

import static org.junit.Assert.assertEquals;
import static org.mockito.ArgumentMatchers.argThat;
import static org.mockito.Mockito.when;

import static org.chromium.chrome.test.pagecontroller.utils.TestUtils.assertLocatorResults;
import static org.chromium.chrome.test.pagecontroller.utils.TestUtils.matchesByDepth;
import static org.chromium.chrome.test.pagecontroller.utils.TestUtils.matchesByField;

import android.content.res.Resources;

import androidx.test.uiautomator.UiDevice;
import androidx.test.uiautomator.UiObject2;

import org.junit.After;
import org.junit.Before;
import org.junit.FixMethodOrder;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.MethodSorters;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;

import java.util.ArrayList;
import java.util.Collections;
import java.util.List;
import java.util.regex.Pattern;

/** Tests for Ui2Locators. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
@FixMethodOrder(MethodSorters.NAME_ASCENDING)
public class Ui2LocatorsTest {
    @Mock UiDevice mDevice;

    @Mock Resources mResources;

    List<UiObject2> mRootAsList;
    List<UiObject2> mChild0And1;
    List<UiObject2> mChild1AsList;
    List<UiObject2> mGrandchildren;
    List<UiObject2> mGrandchild1AsList;

    @Mock UiObject2 mRoot;
    @Mock UiObject2 mChild0;
    @Mock UiObject2 mChild1;
    @Mock UiObject2 mGrandchild0;
    @Mock UiObject2 mGrandchild1;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);

        mRootAsList = Collections.singletonList(mRoot);

        mChild0And1 = new ArrayList<>();
        mChild0And1.add(mChild0);
        mChild0And1.add(mChild1);

        mChild1AsList = Collections.singletonList(mChild1);

        mGrandchildren = new ArrayList<>();
        mGrandchildren.add(mGrandchild0);
        mGrandchildren.add(mGrandchild1);

        mGrandchild1AsList = Collections.singletonList(mGrandchild1);

        when(mDevice.findObjects(argThat(matchesByDepth(0)))).thenReturn(mRootAsList);
        when(mDevice.findObject(argThat(matchesByDepth(2)))).thenReturn(mGrandchild0);
        when(mDevice.findObjects(argThat(matchesByDepth(2)))).thenReturn(mGrandchildren);
        when(mRoot.findObject(argThat(matchesByDepth(2)))).thenReturn(mGrandchild0);
        when(mRoot.findObjects(argThat(matchesByDepth(2)))).thenReturn(mGrandchildren);

        when(mRoot.getChildren()).thenReturn(mChild0And1);
        when(mChild0.getChildren()).thenReturn(mGrandchildren);
        when(mChild1.getChildren()).thenReturn(Collections.emptyList());
    }

    @After
    public void tearDown() {
        Ui2Locators.setResources(null);
    }

    @Test
    public void withChildIndex() {
        Pattern p = Pattern.compile("name");
        stubMocksWithPattern(p, "mRes");
        IUi2Locator locator = Ui2Locators.withChildIndex(0, 1);
        assertEquals(mChild1, locator.locateOne(mDevice));
        assertEquals(mChild1AsList, locator.locateAll(mDevice));
        assertEquals(mGrandchild1, locator.locateOne(mRoot));
        assertEquals(mGrandchild1AsList, locator.locateAll(mRoot));
    }

    @Test
    public void withChildDepth() {
        IUi2Locator locator = Ui2Locators.withChildDepth(2);
        assertLocatorResults(mDevice, mRoot, locator, mGrandchild0, mGrandchildren);
    }

    @Test
    public void withResIdRegex() {
        Pattern p = Pattern.compile("^.*:id/a.*b$");
        stubMocksWithPattern(p, "mRes");
        IUi2Locator locator = Ui2Locators.withResIdRegex("a.*b");
        assertDefaultResults(locator);
    }

    @Test
    public void withAnyResId() {
        Pattern p = Pattern.compile("^.*:id/(a|b)$");
        stubMocksWithPattern(p, "mRes");
        IUi2Locator locator = Ui2Locators.withAnyResId("a", "b");
        assertDefaultResults(locator);
    }

    @Test
    public void withAnyResEntry() {
        stubMocksWithResEntry(123, "someEntry");
        IUi2Locator locator = Ui2Locators.withAnyResEntry(123);
        assertDefaultResults(locator);
    }

    @Test
    public void withResName() {
        Pattern p = Pattern.compile(Pattern.quote("name"));
        stubMocksWithPattern(p, "mRes");
        IUi2Locator locator = Ui2Locators.withResName("name");
        assertDefaultResults(locator);
    }

    @Test
    public void withResNameRegex() {
        Pattern p = Pattern.compile(".*name");
        stubMocksWithPattern(p, "mRes");
        IUi2Locator locator = Ui2Locators.withResNameRegex(".*name");
        assertDefaultResults(locator);
    }

    @Test
    public void withContentDesc() {
        Pattern p = Pattern.compile(Pattern.quote("desc"));
        stubMocksWithPattern(p, "mDesc");
        IUi2Locator locator = Ui2Locators.withContentDesc("desc");
        assertDefaultResults(locator);
    }

    @Test
    public void withContentDescString() {
        stubMocksWithDescString(123, "someDesc");
        IUi2Locator locator = Ui2Locators.withContentDescString(123);
        assertDefaultResults(locator);
    }

    @Test
    public void withText() {
        Pattern p = Pattern.compile(Pattern.quote("text"));
        stubMocksWithPattern(p, "mText");
        IUi2Locator locator = Ui2Locators.withText("text");
        assertDefaultResults(locator);
    }

    @Test
    public void withTextRegex() {
        Pattern p = Pattern.compile(".*text");
        stubMocksWithPattern(p, "mText");
        IUi2Locator locator = Ui2Locators.withTextRegex(".*text");
        assertDefaultResults(locator);
    }

    @Test
    public void withTextContaining() {
        Pattern p = Pattern.compile("^.*" + Pattern.quote("text") + ".*$");
        stubMocksWithPattern(p, "mText");
        IUi2Locator locator = Ui2Locators.withTextContaining("text");
        assertDefaultResults(locator);
    }

    @Test
    public void withTextString() {
        stubMocksWithTextString(123, "someString");
        IUi2Locator locator = Ui2Locators.withTextString(123);
        assertDefaultResults(locator);
    }

    @Test
    public void withClassRegex() {
        Pattern p = Pattern.compile(".*class");
        stubMocksWithPattern(p, "mClazz");
        IUi2Locator locator = Ui2Locators.withClassRegex(".*class");
        assertDefaultResults(locator);
    }

    @Test
    public void withPath() {
        Pattern p = Pattern.compile(".*class");
        stubMocksWithPattern(p, "mClazz");
        IUi2Locator locator0 = Ui2Locators.withClassRegex(".*class");
        IUi2Locator locator1 = Ui2Locators.withChildIndex(1);
        IUi2Locator locator = Ui2Locators.withPath(locator0, locator1);
        assertLocatorResults(mDevice, mRoot, locator, mGrandchild1, mGrandchild1AsList);
    }

    @Test
    public void withPackageName() {
        Pattern p = Pattern.compile(Pattern.quote("package"));
        stubMocksWithPattern(p, "mPkg");
        IUi2Locator locator = Ui2Locators.withPackageName("package");
        assertDefaultResults(locator);
    }

    private void stubMocksWithPattern(Pattern p, String fieldName) {
        when(mDevice.findObjects(argThat(matchesByField(p, fieldName)))).thenReturn(mChild0And1);
        when(mRoot.findObjects(argThat(matchesByField(p, fieldName)))).thenReturn(mChild0And1);
        when(mDevice.findObject(argThat(matchesByField(p, fieldName)))).thenReturn(mChild0);
        when(mRoot.findObject(argThat(matchesByField(p, fieldName)))).thenReturn(mChild0);
    }

    private void stubMocksWithResEntry(int stringId, String stringValue) {
        when(mResources.getResourceEntryName(stringId)).thenReturn(stringValue);
        Ui2Locators.setResources(mResources);
        Pattern p = Pattern.compile("^.*:id/(" + stringValue + ")$");
        stubMocksWithPattern(p, "mRes");
    }

    private void stubMocksWithDescString(int stringId, String stringValue) {
        when(mResources.getString(stringId)).thenReturn(stringValue);
        Ui2Locators.setResources(mResources);
        Pattern p = Pattern.compile(Pattern.quote(stringValue));
        stubMocksWithPattern(p, "mDesc");
    }

    private void stubMocksWithTextString(int stringId, String stringValue) {
        when(mResources.getString(stringId)).thenReturn(stringValue);
        Ui2Locators.setResources(mResources);
        Pattern p = Pattern.compile(Pattern.quote(stringValue));
        stubMocksWithPattern(p, "mText");
    }

    private void assertDefaultResults(IUi2Locator locator) {
        assertLocatorResults(mDevice, mRoot, locator, mChild0, mChild0And1);
    }
}
