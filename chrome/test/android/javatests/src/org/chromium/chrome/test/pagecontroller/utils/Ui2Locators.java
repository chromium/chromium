// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.pagecontroller.utils;

import android.content.res.Resources;
import android.support.test.InstrumentationRegistry;
import android.support.test.uiautomator.By;

import androidx.annotation.IdRes;
import androidx.annotation.NonNull;
import androidx.annotation.VisibleForTesting;

import java.util.regex.Pattern;

/**
 * Locators that find UIObject2 nodes.
 */
public final class Ui2Locators {
    private static Resources sMockResources;

    @VisibleForTesting
    static void setResources(Resources mockResources) {
        sMockResources = mockResources;
    }

    /**
     * Returns a locator that find node(s) matching any one of R.id.* entry names (ignoring the
     * package name).
     *
     * This is the preferred way to locate nodes if the R.id.* is available.
     *
     * @param id             The layout resource id of the corresponding view node.
     * @param additionalIds  Optional additional layout resource ids to match.
     * @return               A locator that will match against the any of the ids.
     */
    public static IUi2Locator withAnyResEntry(@IdRes int id, @IdRes int... additionalIds) {
        return withAnyResId(getResourceEntryName(id), getResourceEntryNames(additionalIds));
    }

    /**
     * Locates the node(s) having a resource id name among a list of names (excluding the
     * package:id/ part).
     *
     * @param firstId       The first id to match against.
     * @param additionalIds Optional ids to match against.
     * @return              A locator that will find node(s) if they match any of the ids.
     */
    public static IUi2Locator withAnyResId(String firstId, String... additionalIds) {
        StringBuilder stringBuilder = new StringBuilder();
        stringBuilder.append("(");
        stringBuilder.append(firstId);
        for (int i = 0; i < additionalIds.length; i++) {
            stringBuilder.append("|" + additionalIds[i]);
        }
        stringBuilder.append(")");
        return withResIdRegex(stringBuilder.toString());
    }

    /**
     * Locates the node(s) having a resource id name that matches a regex (excluding the package:id/
     * part).  Prefer using {@link #withAnyResIdEntry(int, int...)}
     *
     * @param idRegex Regular expression to match ids against.
     * @return        A locator that will find node(s) whose ids match the given regular
     *                expression.
     */
    public static IUi2Locator withResIdRegex(@NonNull String idRegex) {
        return withResNameRegex("^.*:id/" + idRegex + "$");
    }

    /**
     * Locates the node(s) having an exact resource name (including the package:id/ part).
     *
     * @param resourceName The resource name to find.
     * @return             A locator that will find node(s) whose resource name matches.
     */
    public static IUi2Locator withResName(@NonNull String resourceName) {
        return new BySelectorUi2Locator(By.res(resourceName));
    }

    /**
     * Locates the node(s) having a resource name that match the regex (including the package:id/
     * part).
     *
     * @param resourceNameRegex The resource name to find.
     * @return                  A locator that will find node(s) whose resource name matches.
     */
    public static IUi2Locator withResNameRegex(@NonNull String resourceNameRegex) {
        return new BySelectorUi2Locator(By.res(Pattern.compile(resourceNameRegex)));
    }

    /**
     * Locates the node(s) having a text string from resource id.
     *
     * This is the preferred way to locate nodes by text content if the string
     * resource id is available.
     *
     * @param idString  The id of the string.
     * @return          A locator that will find the node(s) with text that matches the
     *                  string identified by the given id.
     */
    public static IUi2Locator withTextString(int idString) {
        return new BySelectorUi2Locator(By.text(getResourceStringName(idString)));
    }

    /**
     * Locates the node(s) by position in the siblings list, recursively if more indices are
     * specified.
     *
     * The order of the nodes is determined by the implementation of UiObject2.findObjects,
     * but not documented, at present it's pre-order.
     *
     * @param childIndex        The index of the child.
     * @param descendantIndices Optional additional indices of descendants.
     * @return                  A locator that will locate a child or descendant node by traversing
     *                          the list of child indices.
     */
    public static IUi2Locator withChildIndex(int childIndex, int... descendantIndices) {
        return new ChildIndexUi2Locator(childIndex, descendantIndices);
    }

    /**
     * Locates the node(s) at a certain depth.
     *
     * @param depth The depth.
     * @return      A locator that will locate the child(ren) at the given depth.
     */
    public static IUi2Locator withChildDepth(int depth) {
        return new BySelectorUi2Locator(By.depth(depth));
    }

    /**
     * Locates the node(s) having content description from string resource id.
     *
     * @param stringId The string resource id.
     * @return         A locator that will find the node(s) with the given content
     *                 description string id.
     *
     * @see <a
     *         href="https://developer.android.com/reference/android/view/View.html#getContentDescription()">getContentDescription</a>
     */
    public static IUi2Locator withContentDescString(int stringId) {
        return new BySelectorUi2Locator(By.desc(getResourceStringName(stringId)));
    }

    /**
     * Locates the node(s) having an exact content description.
     *
     * @param desc The content description to match against.
     * @return     A locator that will find the node(s) with the given content description.
     *
     * @see <a
     *         href="https://developer.android.com/reference/android/view/View.html#getContentDescription()">getContentDescription</a>
     */
    public static IUi2Locator withContentDesc(@NonNull String desc) {
        return new BySelectorUi2Locator(By.desc(desc));
    }

    /**
     * Locates the node(s) having an exact text string.
     *
     * @param text The text to match.
     * @return     A locator that will find the node(s) having the given text.
     *
     * @see <a
     *         href="https://developer.android.com/reference/android/widget/TextView.html#getText()">getText</a>
     */
    public static IUi2Locator withText(@NonNull String text) {
        return new BySelectorUi2Locator(By.text(text));
    }

    /**
     * Locates the node(s) having a text string that matches a regex.
     *
     * @param textRegex The regular expression to match the text against.
     * @return          A locator that will find the node(s) with text that matches the
     *                  given regular expression.
     *
     * @see <a
     *         href="https://developer.android.com/reference/android/widget/TextView.html#getText()">getText</a>
     */
    public static IUi2Locator withTextRegex(@NonNull String textRegex) {
        return new BySelectorUi2Locator(By.text(Pattern.compile(textRegex)));
    }

    /**
     * Locates the node(s) having a text string that contains a substring.
     *
     * @param subText Substring to search in the text field.
     * @return        A locator that will find node(s) with text that contains the given string.
     *
     * @see <a
     *         href="https://developer.android.com/reference/android/widget/TextView.html#getText()">getText</a>
     */
    public static IUi2Locator withTextContaining(@NonNull String subText) {
        return new BySelectorUi2Locator((By.textContains(subText)));
    }

    /**
     * Locates the node(s) having a class name that matches a regex.
     *
     * @param regex Regular expression for the class name.
     * @return      A locator that will find node(s) with class name that matches
     *              the given regular expression.
     */
    public static IUi2Locator withClassRegex(@NonNull String regex) {
        return new BySelectorUi2Locator((By.clazz(Pattern.compile(regex))));
    }

    /**
     * Locates the ith node(s) found by another locator.
     *
     * @param index   The value of i.
     * @param locator First locator in the chain.
     * @return        A locator that will find ith node in the results of locator.
     */
    public static IUi2Locator withIndex(int index, @NonNull IUi2Locator locator) {
        return new IndexUi2Locator(index, locator);
    }

    /**
     * Locates the node(s) matching the chain of locators.
     *
     * @param locator            First locator in the chain.
     * @param additionalLocators Optional, additional locators in the chain.
     * @return                   A locator that will find node(s) mathcing the chain of
     *                           locators.
     */
    public static IUi2Locator withPath(
            @NonNull IUi2Locator locator, IUi2Locator... additionalLocators) {
        return new PathUi2Locator(locator, additionalLocators);
    }

    /**
     * Locates the node(s) having an exact package name.
     *
     * @param packageName Exact package name to match.
     * @return            A locator that will find node(s) having the packageName.
     */
    public static IUi2Locator withPackageName(@NonNull String packageName) {
        return new BySelectorUi2Locator(By.pkg(packageName));
    }

    /**
     * This converts the integer resource ids to string resource entry names so
     * that UIAutomator can be used to located them.
     *
     * @param id The layout resource id of the corresponding view node.
     * @return   String resource entry name for id.
     */
    private static String getResourceEntryName(@IdRes int id) {
        return getTargetResources().getResourceEntryName(id);
    }

    /**
     * This converts the integer resource ids to string resource entry names so
     * that UIAutomator can be used to located them.
     *
     * @param ids The layout resource id of the corresponding view node.
     * @return    Array of string resource entry names for ids.
     */
    private static String[] getResourceEntryNames(@IdRes int... ids) {
        String[] names = new String[(ids.length)];
        for (int i = 0; i < ids.length; i++) {
            names[i] = getResourceEntryName(ids[i]);
        }
        return names;
    }

    /**
     * This converts the integer string id to the localized string.
     *
     * @param idString The string id.
     * @return         String value of idString.
     */
    private static String getResourceStringName(int idString) {
        return getTargetResources().getString(idString);
    }

    private static Resources getTargetResources() {
        if (sMockResources == null) {
            return InstrumentationRegistry.getTargetContext().getResources();
        } else {
            return sMockResources;
        }
    }
}
