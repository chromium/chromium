// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.transit.hub;

import static androidx.test.espresso.action.ViewActions.replaceText;
import static androidx.test.espresso.matcher.ViewMatchers.isAssignableFrom;
import static androidx.test.espresso.matcher.ViewMatchers.withContentDescription;
import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.hamcrest.CoreMatchers.allOf;
import static org.hamcrest.CoreMatchers.startsWith;

import static org.chromium.base.ThreadUtils.runOnUiThreadBlocking;
import static org.chromium.base.test.transit.ViewElement.inDialogOption;
import static org.chromium.base.test.transit.ViewElement.newOptions;
import static org.chromium.base.test.transit.ViewSpec.viewSpec;

import android.content.Context;
import android.view.View;
import android.widget.EditText;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.hamcrest.Matcher;

import org.chromium.base.Token;
import org.chromium.base.test.transit.Element;
import org.chromium.base.test.transit.Facility;
import org.chromium.base.test.transit.ViewElement;
import org.chromium.base.test.transit.ViewSpec;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabGroupColorUtils;
import org.chromium.chrome.browser.tabmodel.TabGroupModelFilter;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelUtils;
import org.chromium.chrome.test.R;
import org.chromium.chrome.test.transit.ChromeActivityTabModelBoundStation;
import org.chromium.chrome.test.transit.SoftKeyboardFacility;
import org.chromium.chrome.test.transit.tabmodel.TabGroupCreatedCondition;
import org.chromium.chrome.test.transit.tabmodel.TabGroupUtil;
import org.chromium.chrome.test.util.TabBinningUtil;
import org.chromium.components.tab_groups.TabGroupColorId;
import org.chromium.components.tab_groups.TabGroupColorPickerUtils;

import java.util.List;

/**
 * Dialog that appears when a new tab group is created to name the group and pick a color.
 *
 * @param <HostStationT> the type of station this is scoped to.
 */
public class NewTabGroupDialogFacility<
                HostStationT extends ChromeActivityTabModelBoundStation<ChromeTabbedActivity>>
        extends Facility<HostStationT> {
    private final @Nullable @TabGroupColorId Integer mSelectedColor;
    private final SoftKeyboardFacility mSoftKeyboard;
    public ViewElement<View> dialogElement;
    public ViewElement<View> titleInputElement;
    public ViewElement<View>[] colorElements;
    public ViewElement<View> doneButtonElement;
    private @Nullable String mTitle;
    private @Nullable List<Integer> mTabIdsToGroup;

    /** Constructor. Expects no particular title or selected color. */
    public NewTabGroupDialogFacility(SoftKeyboardFacility softKeyboard) {
        this(/* tabIdsToGroup= */ null, /* title= */ null, /* selectedColor= */ null, softKeyboard);
    }

    /** Constructor. Expects no particular title or selected color. */
    public NewTabGroupDialogFacility(
            List<Integer> tabIdsToGroup, SoftKeyboardFacility softKeyboard) {
        this(
                tabIdsToGroup,
                TabGroupUtil.getNumberOfTabsString(tabIdsToGroup.size()),
                /* selectedColor= */ null,
                softKeyboard);
    }

    /** Constructor. Expects a specific title and selected color. */
    public NewTabGroupDialogFacility(
            List<Integer> tabIdsToGroup,
            String title,
            @Nullable @TabGroupColorId Integer selectedColor,
            SoftKeyboardFacility softKeyboard) {
        mTabIdsToGroup = tabIdsToGroup;
        mTitle = title;
        mSelectedColor = selectedColor;
        mSoftKeyboard = softKeyboard;
    }

    @Override
    public void declareExtraElements() {
        // Handles the case for when a new tab group is created on showing this dialog.
        if (mTabIdsToGroup == null) {
            initTabGroupCreatedCondition();
        } else {
            titleInputElement = declareView(createTitleViewSpec(), inDialogOption());
        }

        dialogElement =
                declareView(
                        withId(R.id.visual_data_dialog_layout),
                        newOptions().inDialog().displayingAtLeast(80).build());
        declareView(
                viewSpec(allOf(withId(R.id.visual_data_dialog_title), withText("New tab group"))),
                inDialogOption());

        // TODO(crbug.com/346377124): Partially cut off in android_30_google_apis_x86.textpb
        declareView(withId(R.id.color_picker_container), ViewElement.displayingAtLeastOption(50));
        @TabGroupColorId List<Integer> colors = TabGroupColorUtils.getTabGroupColorIdList();
        // Only the first 5 colors are displayed reliably when the soft keyboard opens.
        colorElements = new ViewElement[5];
        for (int i = 0; i < 5; i++) {
            @TabGroupColorId Integer color = colors.get(i);
            if (mSelectedColor != null) {
                colorElements[i] =
                        declareView(
                                colorPickerIconSpec(color, color.equals(mSelectedColor)),
                                newOptions().inDialog().unscoped().displayingAtLeast(60).build());
            } else {
                colorElements[i] =
                        declareView(
                                colorPickerIconSpec(color, /* selected= */ null),
                                newOptions().inDialog().unscoped().displayingAtLeast(60).build());
            }
        }

        doneButtonElement = declareView(withId(R.id.positive_button), inDialogOption());
    }

    @NonNull
    private ViewSpec<View> createTitleViewSpec() {
        return viewSpec(
                withId(R.id.title_input_text), isAssignableFrom(EditText.class), withText(mTitle));
    }

    private void initTabGroupCreatedCondition() {
        Element<Token> tabGroupIdElement =
                declareEnterConditionAsElement(
                        new TabGroupCreatedCondition(mHostStation.tabGroupModelFilterElement));

        declareElementFactory(
                tabGroupIdElement,
                delayedElements -> {
                    TabGroupModelFilter filter = mHostStation.tabGroupModelFilterElement.value();
                    List<Tab> tabsInGroup =
                            runOnUiThreadBlocking(
                                    () -> filter.getTabsInGroup(tabGroupIdElement.value()));
                    mTabIdsToGroup = TabModelUtils.getTabIds(tabsInGroup);
                    mTitle = TabGroupUtil.getNumberOfTabsString(mTabIdsToGroup.size());
                    titleInputElement =
                            delayedElements.declareView(createTitleViewSpec(), inDialogOption());
                });
    }

    private ViewSpec<View> colorPickerIconSpec(
            @TabGroupColorId Integer color, @Nullable Boolean selected) {
        Context context = mHostStation.getActivity();
        String colorName =
                context.getString(
                        TabGroupColorPickerUtils.getTabGroupColorPickerItemColorAccessibilityString(
                                color));
        Matcher<View> contentDescriptionMatcher;
        if (selected != null) {
            contentDescriptionMatcher =
                    withContentDescription(
                            colorName + " " + (selected ? "Selected" : "Not selected"));
        } else {
            contentDescriptionMatcher = withContentDescription(startsWith(colorName));
        }
        return viewSpec(withId(R.id.color_picker_icon), contentDescriptionMatcher);
    }

    /** Input a new tab group name. */
    public NewTabGroupDialogFacility<HostStationT> inputName(String newTabGroupName) {
        // An empty name causes warning text to show up which could push the color picker container
        // out of view for small screen devices, so dismiss the keyboard.
        if (newTabGroupName.isEmpty()) {
            ensureSoftKeyboardClosed();
        }

        return titleInputElement
                .performViewActionTo(replaceText(newTabGroupName))
                .exitFacilityAnd()
                .enterFacility(
                        new NewTabGroupDialogFacility<>(
                                mTabIdsToGroup, newTabGroupName, mSelectedColor, mSoftKeyboard));
    }

    /** Select a color. */
    public NewTabGroupDialogFacility<HostStationT> pickColor(@TabGroupColorId int newColor) {
        return colorElements[newColor]
                .clickTo()
                .exitFacilityAnd()
                .enterFacility(
                        new NewTabGroupDialogFacility<>(
                                mTabIdsToGroup, mTitle, newColor, mSoftKeyboard));
    }

    /** Press "Done" to confirm the tab group name and color. */
    public TabSwitcherGroupCardFacility pressDone() {
        ensureSoftKeyboardClosed();

        // The reason we can pass an expected card index is because the tab group has already been
        // created.
        TabModel currentModel = mHostStation.getTabModel();
        int expectedCardIndex =
                runOnUiThreadBlocking(
                        () -> TabBinningUtil.getBinIndex(currentModel, mTabIdsToGroup));
        return doneButtonElement
                .clickTo()
                .exitFacilityAnd()
                .enterFacility(
                        new TabSwitcherGroupCardFacility(
                                expectedCardIndex, mTabIdsToGroup, mTitle));
    }

    /**
     * Press "Done" to confirm the tab group name and color. This method should only be called when
     * a TabGroupDialog will open on clicking 'Done'.
     */
    public TabGroupDialogFacility<HostStationT> pressDoneAsPartOfFlow() {
        ensureSoftKeyboardClosed();

        // The reason we can pass an expected card index is because the tab group has already been
        // created.
        return doneButtonElement
                .clickTo()
                .exitFacilityAnd()
                .enterFacility(new TabGroupDialogFacility<>(mTabIdsToGroup));
    }

    /**
     * Press "Done" to confirm the tab group name and color. This method should only be called when
     * shown as part of a TabGroupListBottomSheet flow.
     */
    public void pressDoneToExit() {
        ensureSoftKeyboardClosed();
        doneButtonElement.clickTo().exitFacility();
    }

    /** Press "Done" to confirm the tab group name and color, but no-op from an invalid title. */
    public NewTabGroupDialogFacility<HostStationT> pressDoneWithInvalidTitle() {
        ensureSoftKeyboardClosed();

        return doneButtonElement
                .clickTo()
                .exitFacilityAnd()
                .withPossiblyAlreadyFulfilled()
                .enterFacility(
                        new NewTabGroupDialogFacility<>(
                                mTabIdsToGroup, mTitle, mSelectedColor, mSoftKeyboard));
    }

    /** Press the system backpress to confirm the tab group name and color. */
    public TabSwitcherGroupCardFacility pressBack() {
        ensureSoftKeyboardClosed();

        // The reason we can pass an expected card index is because the tab group has already been
        // created.
        TabModel currentModel = mHostStation.getTabModel();
        int expectedCardIndex =
                runOnUiThreadBlocking(
                        () -> TabBinningUtil.getBinIndex(currentModel, mTabIdsToGroup));
        return pressBackTo()
                .exitFacilityAnd()
                .enterFacility(
                        new TabSwitcherGroupCardFacility(
                                expectedCardIndex, mTabIdsToGroup, mTitle));
    }

    private void ensureSoftKeyboardClosed() {
        if (mSoftKeyboard.getPhase() == Phase.ACTIVE) {
            mSoftKeyboard.close(dialogElement);
        } else if (mSoftKeyboard.getPhase() == Phase.FINISHED) {
            // Do nothing as the soft keyboard has already been closed
        } else {
            throw new IllegalArgumentException(
                    "SoftKeyboardFacility is in phase " + mSoftKeyboard.getPhase());
        }
    }
}
