// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.transit;

import static org.chromium.base.test.transit.Condition.awaiting;
import static org.chromium.base.test.transit.Condition.fulfilled;

import org.chromium.base.test.transit.Element;
import org.chromium.base.test.transit.SimpleConditions;
import org.chromium.base.test.transit.Station;
import org.chromium.chrome.browser.app.ChromeActivity;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileProvider;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.test.transit.tabmodel.TabModelCondition;
import org.chromium.chrome.test.transit.tabmodel.TabModelSelectorCondition;

/** Base class for |Station<ChromeActivity>| that are related to a specific TabModel. */
public class ChromeActivityTabModelBoundStation<HostActivity extends ChromeActivity>
        extends Station<HostActivity> {
    protected final boolean mIsIncognito;

    public final Element<Profile> originalProfileElement;
    public final Element<TabModelSelector> tabModelSelectorElement;
    public final Element<TabModel> tabModelElement;

    public ChromeActivityTabModelBoundStation(
            Class<HostActivity> activityClass, boolean isIncognito) {
        super(activityClass);
        mIsIncognito = isIncognito;
        originalProfileElement =
                declareEnterConditionAsElement(
                        SimpleConditions.uiThreadConditionWithResult(
                                "Activity original Profile is available",
                                mActivityElement,
                                activity -> {
                                    ProfileProvider provider =
                                            activity.getProfileProviderSupplier().get();
                                    if (provider == null) {
                                        return awaiting("Activity has no ProfileProvider")
                                                .withoutResult();
                                    }
                                    Profile profile = provider.getOriginalProfile();
                                    if (profile == null) {
                                        return awaiting("Original Profile not available yet")
                                                .withoutResult();
                                    }
                                    return fulfilled("baseName=%s", profile.getBaseName())
                                            .withResult(profile);
                                }));
        tabModelSelectorElement =
                declareEnterConditionAsElement(new TabModelSelectorCondition(mActivityElement));
        tabModelElement =
                declareEnterConditionAsElement(
                        new TabModelCondition(tabModelSelectorElement, mIsIncognito));
    }

    /** Convenience method for |originalProfileElement.value()|. */
    public Profile getOriginalProfile() {
        return originalProfileElement.value();
    }

    /** Convenience method for |tabModelElement.get()|. */
    public TabModel getTabModel() {
        return tabModelElement.value();
    }

    /** Convenience method for |tabModelSelectorElement.get()|. */
    public TabModelSelector getTabModelSelector() {
        return tabModelSelectorElement.value();
    }

    /** Whether bound to an incognito TabModel. */
    public boolean isIncognito() {
        return mIsIncognito;
    }
}
