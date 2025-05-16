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

import static org.chromium.base.test.transit.ViewSpec.viewSpec;

import android.content.Context;
import android.view.View;
import android.widget.EditText;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.test.espresso.Espresso;

import org.hamcrest.Matcher;

import org.chromium.base.Token;
import org.chromium.base.test.transit.Element;
import org.chromium.base.test.transit.Facility;
import org.chromium.base.test.transit.Station;
import org.chromium.base.test.transit.Transition;
import org.chromium.base.test.transit.ViewElement;
import org.chromium.base.test.transit.ViewSpec;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabGroupColorUtils;
import org.chromium.chrome.browser.tabmodel.TabGroupModelFilter;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelUtils;
import org.chromium.chrome.browser.tasks.tab_management.ColorPickerUtils;
import org.chromium.chrome.test.R;
import org.chromium.chrome.test.transit.SoftKeyboardFacility;
import org.chromium.chrome.test.transit.tabmodel.TabGroupCreatedCondition;
import org.chromium.chrome.test.transit.tabmodel.TabGroupUtil;
import org.chromium.chrome.test.util.TabBinningUtil;
import org.chromium.components.tab_groups.TabGroupColorId;

import java.util.List;

/**
 * Dialog that appears when a new tab group is created to name the group and pick a color.
 *
 * @param <HostStationT> the type of station this is scoped to.
 */
public class NewTabGroupDialogFacility<HostStationT extends Station<ChromeTabbedActivity>>
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
            titleInputElement = declareView(createTitleViewSpec());
        }

        dialogElement =
                declareView(
                        withId(R.id.visual_data_dialog_layout),
                        ViewElement.displayingAtLeastOption(80));
        declareView(
                viewSpec(allOf(withId(R.id.visual_data_dialog_title), withText("New tab group"))));

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
                                ViewElement.newOptions().unscoped().displayingAtLeast(60).build());
            } else {
                colorElements[i] =
                        declareView(
                                colorPickerIconSpec(color, /* selected= */ null),
                                ViewElement.newOptions().unscoped().displayingAtLeast(60).build());
            }
        }

        doneButtonElement = declareView(withId(R.id.positive_button));
    }

    @NonNull
    private ViewSpec<View> createTitleViewSpec() {
        return viewSpec(
                withId(R.id.title_input_text), isAssignableFrom(EditText.class), withText(mTitle));
    }

    private void initTabGroupCreatedCondition() {
        ChromeTabbedActivity activity = mHostStation.getActivity();
        boolean isIncognito = activity.getCurrentTabModel().isIncognitoBranded();
        TabGroupModelFilter filter =
                activity.getTabModelSelector()
                        .getTabGroupModelFilterProvider()
                        .getTabGroupModelFilter(isIncognito);
        Element<Token> tabGroupIdElement =
                declareEnterConditionAsElement(
                        new TabGroupCreatedCondition(
                                isIncognito, activity.getTabModelSelectorSupplier()));

        declareElementFactory(
                tabGroupIdElement,
                delayedElements -> {
                    List<Tab> tabsInGroup = filter.getTabsInGroup(tabGroupIdElement.get());
                    mTabIdsToGroup = TabModelUtils.getTabIds(tabsInGroup);
                    mTitle = TabGroupUtil.getNumberOfTabsString(mTabIdsToGroup.size());
                    titleInputElement = delayedElements.declareView(createTitleViewSpec());
                });
    }

    private ViewSpec<View> colorPickerIconSpec(
            @TabGroupColorId Integer color, @Nullable Boolean selected) {
        Context context = mHostStation.getActivity();
        String colorName =
                context.getString(
                        ColorPickerUtils.getTabGroupColorPickerItemColorAccessibilityString(color));
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

        return mHostStation.swapFacilitySync(
                this,
                new NewTabGroupDialogFacility<>(
                        mTabIdsToGroup, newTabGroupName, mSelectedColor, mSoftKeyboard),
                titleInputElement.getPerformTrigger(replaceText(newTabGroupName)));
    }

    /** Select a color. */
    public NewTabGroupDialogFacility<HostStationT> pickColor(@TabGroupColorId int newColor) {
        return mHostStation.swapFacilitySync(
                this,
                new NewTabGroupDialogFacility<>(mTabIdsToGroup, mTitle, newColor, mSoftKeyboard),
                colorElements[newColor].getClickTrigger());
    }

    /** Press "Done" to confirm the tab group name and color. */
    public TabSwitcherGroupCardFacility pressDone() {
        ensureSoftKeyboardClosed();

        // The reason we can pass an expected card index is because the tab group has already been
        // created.
        TabModel currentModel = mHostStation.getActivity().getCurrentTabModel();
        int expectedCardIndex = TabBinningUtil.getBinIndex(currentModel, mTabIdsToGroup);
        return mHostStation.swapFacilitySync(
                this,
                new TabSwitcherGroupCardFacility(expectedCardIndex, mTabIdsToGroup, mTitle),
                doneButtonElement.getClickTrigger());
    }

    /**
     * Press "Done" to confirm the tab group name and color. This method should only be called when
     * a TabGroupDialog will open on clicking 'Done'.
     */
    public TabGroupDialogFacility<HostStationT> pressDoneAsPartOfFlow() {
        ensureSoftKeyboardClosed();

        // The reason we can pass an expected card index is because the tab group has already been
        // created.
        TabModel currentModel = mHostStation.getActivity().getCurrentTabModel();
        return mHostStation.swapFacilitySync(
                this,
                new TabGroupDialogFacility<>(mTabIdsToGroup, currentModel.isIncognitoBranded()),
                doneButtonElement.getClickTrigger());
    }

    /**
     * Press "Done" to confirm the tab group name and color. This method should only be called when
     * shown as part of a TabGroupListBottomSheet flow.
     */
    public void pressDoneToExit() {
        ensureSoftKeyboardClosed();
        mHostStation.exitFacilitySync(this, doneButtonElement.getClickTrigger());
    }

    /** Press "Done" to confirm the tab group name and color, but no-op from an invalid title. */
    public NewTabGroupDialogFacility<HostStationT> pressDoneWithInvalidTitle() {
        ensureSoftKeyboardClosed();

        return mHostStation.swapFacilitySync(
                this,
                new NewTabGroupDialogFacility<>(
                        mTabIdsToGroup, mTitle, mSelectedColor, mSoftKeyboard),
                Transition.possiblyAlreadyFulfilledOption(),
                doneButtonElement.getClickTrigger());
    }

    /** Press the system backpress to confirm the tab group name and color. */
    public TabSwitcherGroupCardFacility pressBack() {
        ensureSoftKeyboardClosed();

        // The reason we can pass an expected card index is because the tab group has already been
        // created.
        TabModel currentModel = mHostStation.getActivity().getCurrentTabModel();
        int expectedCardIndex = TabBinningUtil.getBinIndex(currentModel, mTabIdsToGroup);
        return mHostStation.swapFacilitySync(
                this,
                new TabSwitcherGroupCardFacility(expectedCardIndex, mTabIdsToGroup, mTitle),
                Espresso::pressBack);
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
