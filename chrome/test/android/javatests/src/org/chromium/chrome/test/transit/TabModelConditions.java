// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.transit;

import org.chromium.base.test.transit.Condition;
import org.chromium.base.test.transit.ConditionStatus;
import org.chromium.base.test.transit.UiThreadCondition;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;

/** {@link Condition}s regarding the state of TabModels. */
public class TabModelConditions {
    /** Fulfilled when no regular tabs are open. */
    public static Condition noRegularTabsExist(ChromeTabbedActivityTestRule testRule) {
        return new NoTabsExistCondition(testRule, /* incognito= */ false);
    }

    /** Fulfilled when no incognito tabs are open. */
    public static Condition noIncognitoTabsExist(ChromeTabbedActivityTestRule testRule) {
        return new NoTabsExistCondition(testRule, /* incognito= */ true);
    }

    /** Fulfilled when one or more regular tabs are open. */
    public static Condition anyRegularTabsExist(ChromeTabbedActivityTestRule testRule) {
        return new AnyTabsExistCondition(testRule, /* incognito= */ false);
    }

    /** Fulfilled when one or more incognito tabs are open. */
    public static Condition anyIncognitoTabsExist(ChromeTabbedActivityTestRule testRule) {
        return new AnyTabsExistCondition(testRule, /* incognito= */ true);
    }

    private static class NoTabsExistCondition extends UiThreadCondition {

        private final ChromeTabbedActivityTestRule mChromeTabbedActivityTestRule;
        private final boolean mIncognito;
        private final String mTabType;

        public NoTabsExistCondition(
                ChromeTabbedActivityTestRule chromeTabbedActivityTestRule, boolean incognito) {
            mChromeTabbedActivityTestRule = chromeTabbedActivityTestRule;
            mIncognito = incognito;
            mTabType = incognito ? "incognito" : "regular";
        }

        @Override
        protected ConditionStatus checkWithSuppliers() {
            int tabCount = mChromeTabbedActivityTestRule.tabsCount(mIncognito);
            return whether(tabCount == 0, "%d %s tabs", tabCount, mTabType);
        }

        @Override
        public String buildDescription() {
            return String.format("No %s tabs exist", mTabType);
        }
    }

    private static class AnyTabsExistCondition extends UiThreadCondition {

        private final ChromeTabbedActivityTestRule mChromeTabbedActivityTestRule;
        private final boolean mIncognito;
        private final String mTabType;

        public AnyTabsExistCondition(
                ChromeTabbedActivityTestRule chromeTabbedActivityTestRule, boolean incognito) {
            mChromeTabbedActivityTestRule = chromeTabbedActivityTestRule;
            mIncognito = incognito;
            mTabType = incognito ? "incognito" : "regular";
        }

        @Override
        protected ConditionStatus checkWithSuppliers() {
            int tabCount = mChromeTabbedActivityTestRule.tabsCount(mIncognito);
            return whether(tabCount > 0, "%d %s tabs", tabCount, mTabType);
        }

        @Override
        public String buildDescription() {
            return String.format("Any %s tabs exist", mTabType);
        }
    }
}
