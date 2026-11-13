// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.transit.toolbar;

import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withId;

import static org.hamcrest.CoreMatchers.allOf;

import android.view.View;

import org.chromium.base.test.transit.Facility;
import org.chromium.base.test.transit.OptionalViewElement;
import org.chromium.base.test.transit.ViewElement;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.toolbar.top.ToolbarControlContainer;
import org.chromium.chrome.test.transit.page.CtaPageStation;

/**
 * The browsing toolbar, shown and interactable.
 *
 * <p>Entered by default with every {@link CtaPageStation}.
 */
@NullMarked
public class ToolbarFacility extends Facility<CtaPageStation> {
    public final ViewElement<ToolbarControlContainer> toolbarElement;
    public final ViewElement<View> tabSwitcherButtonElement;
    public final ViewElement<View> menuButtonElement;
    public final OptionalViewElement<View> homeButtonElement;
    public final OptionalViewElement<View> optionalToolbarMicButtonElement;
    public final OptionalViewElement<View> signinButtonElement;

    public ToolbarFacility() {
        super("ToolbarFacility");

        // TODO(crbug.com/41497463): These should be scoped, but for now they need to be unscoped
        // since they unintentionally still exist in the non-Hub tab switcher. They are mostly
        // occluded by the tab switcher toolbar, but at least the tab_switcher_button is still
        // visible.
        toolbarElement =
                declareView(
                        ToolbarControlContainer.class,
                        withId(R.id.control_container),
                        ViewElement.unscopedOption());
        // These should be unscoped because they can be present in both the top toolbar and the
        // bottom bar.
        tabSwitcherButtonElement =
                declareView(
                        View.class,
                        allOf(withId(R.id.tab_switcher_button), isDisplayed()),
                        ViewElement.unscopedOption());

        menuButtonElement =
                declareView(
                        View.class,
                        allOf(withId(R.id.menu_button), isDisplayed()),
                        ViewElement.unscopedOption());

        // The home button may not appear in tablets if the available screen size is too small.
        homeButtonElement =
                declareOptionalView(
                        allOf(withId(R.id.home_button), isDisplayed()),
                        ViewElement.unscopedOption());

        // The optional toolbar mic button specifically.
        optionalToolbarMicButtonElement = declareOptionalView(CtaPageStation.TOOLBAR_MIC_BUTTON);

        // The sign-in button only appears when the toolbar sign-in button feature is enabled.
        signinButtonElement =
                declareOptionalView(withId(R.id.signin_button), ViewElement.unscopedOption());
    }
}
