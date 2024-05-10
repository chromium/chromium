// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.transit;

import org.chromium.base.supplier.Supplier;
import org.chromium.base.test.transit.Condition;
import org.chromium.base.test.transit.ConditionStatus;
import org.chromium.base.test.transit.UiThreadCondition;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;

/** {@link Condition}s regarding the state of TabModels. */
public class TabModelConditions {
    /** Fulfilled when no regular tabs are open. */
    public static Condition noRegularTabsExist(
            Supplier<TabModelSelector> tabModelSelectorSupplier) {
        return new NoTabsExistCondition(tabModelSelectorSupplier, /* incognito= */ false);
    }

    /** Fulfilled when no incognito tabs are open. */
    public static Condition noIncognitoTabsExist(
            Supplier<TabModelSelector> tabModelSelectorSupplier) {
        return new NoTabsExistCondition(tabModelSelectorSupplier, /* incognito= */ true);
    }

    /** Fulfilled when one or more regular tabs are open. */
    public static Condition anyRegularTabsExist(
            Supplier<TabModelSelector> tabModelSelectorSupplier) {
        return new AnyTabsExistCondition(tabModelSelectorSupplier, /* incognito= */ false);
    }

    /** Fulfilled when one or more incognito tabs are open. */
    public static Condition anyIncognitoTabsExist(
            Supplier<TabModelSelector> tabModelSelectorSupplier) {
        return new AnyTabsExistCondition(tabModelSelectorSupplier, /* incognito= */ true);
    }

    private static class NoTabsExistCondition extends UiThreadCondition {
        private final Supplier<TabModelSelector> mTabModelSelectorSupplier;
        private final boolean mIncognito;
        private final String mTabType;

        public NoTabsExistCondition(
                Supplier<TabModelSelector> tabModelSelectorSupplier, boolean incognito) {
            mTabModelSelectorSupplier =
                    dependOnSupplier(tabModelSelectorSupplier, "TabModelSelector");
            mIncognito = incognito;
            mTabType = incognito ? "incognito" : "regular";
        }

        @Override
        protected ConditionStatus checkWithSuppliers() {
            int tabCount = mTabModelSelectorSupplier.get().getModel(mIncognito).getCount();
            return whether(tabCount == 0, "%d %s tabs", tabCount, mTabType);
        }

        @Override
        public String buildDescription() {
            return String.format("No %s tabs exist", mTabType);
        }
    }

    private static class AnyTabsExistCondition extends UiThreadCondition {
        private final Supplier<TabModelSelector> mTabModelSelectorSupplier;
        private final boolean mIncognito;
        private final String mTabType;

        public AnyTabsExistCondition(
                Supplier<TabModelSelector> tabModelSelectorSupplier, boolean incognito) {
            mTabModelSelectorSupplier =
                    dependOnSupplier(tabModelSelectorSupplier, "TabModelSelector");
            mIncognito = incognito;
            mTabType = incognito ? "incognito" : "regular";
        }

        @Override
        protected ConditionStatus checkWithSuppliers() {
            int tabCount = mTabModelSelectorSupplier.get().getModel(mIncognito).getCount();
            return whether(tabCount > 0, "%d %s tabs", tabCount, mTabType);
        }

        @Override
        public String buildDescription() {
            return String.format("Any %s tabs exist", mTabType);
        }
    }
}
