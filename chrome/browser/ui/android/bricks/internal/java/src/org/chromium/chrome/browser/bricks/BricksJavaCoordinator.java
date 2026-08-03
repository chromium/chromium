// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bricks;

import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.bricks.internal.R;
import org.chromium.chrome.browser.bricks.switches.ComposeMaterialSwitchWithText;
import org.chromium.chrome.browser.bricks.switches.ComposeMaterialSwitchWithTitleAndSummary;
import org.chromium.components.browser_ui.widget.MaterialSwitchWithText;
import org.chromium.components.browser_ui.widget.MaterialSwitchWithTitleAndSummary;

/**
 * Java implementation of BricksCoordinator for chrome://bricks-java native page. Uses an XML layout
 * with embedded Compose switch variants (ComposeMaterialSwitchWithText &
 * ComposeMaterialSwitchWithTitleAndSummary) alongside native Java View switches, sharing identical
 * Java APIs.
 */
@NullMarked
public class BricksJavaCoordinator implements BricksCoordinatorInterface {
    private final View mView;

    public BricksJavaCoordinator(Context context) {
        mView = LayoutInflater.from(context).inflate(R.layout.bricks_java_view, null);

        // Interactive Enabled Java Switch with Text
        MaterialSwitchWithText javaSwitchText = mView.findViewById(R.id.java_switch_text);
        javaSwitchText.setChecked(true);
        javaSwitchText.setOnCheckedChangeListener(
                (buttonView, isChecked) ->
                        javaSwitchText.setText(
                                context.getString(
                                        isChecked
                                                ? R.string.bricks_java_switch_enabled_on
                                                : R.string.bricks_java_switch_enabled_off)));

        // Disabled On Java Switch with Text
        MaterialSwitchWithText javaSwitchTextDisabledOn =
                mView.findViewById(R.id.java_switch_text_disabled_on);
        javaSwitchTextDisabledOn.setChecked(true);
        javaSwitchTextDisabledOn.setEnabled(false);

        // Disabled Off Java Switch with Text
        MaterialSwitchWithText javaSwitchTextDisabledOff =
                mView.findViewById(R.id.java_switch_text_disabled_off);
        javaSwitchTextDisabledOff.setChecked(false);
        javaSwitchTextDisabledOff.setEnabled(false);

        // Interactive Enabled Java Switch with Title & Summary
        MaterialSwitchWithTitleAndSummary javaSwitchTitle =
                mView.findViewById(R.id.java_switch_title_summary);
        javaSwitchTitle.setTitleText(context.getString(R.string.bricks_java_switch_enabled_on));
        javaSwitchTitle.setSummaryText(context.getString(R.string.bricks_java_switch_summary));
        javaSwitchTitle.setChecked(true);
        javaSwitchTitle.setOnCheckedChangeListener(
                (buttonView, isChecked) ->
                        javaSwitchTitle.setTitleText(
                                context.getString(
                                        isChecked
                                                ? R.string.bricks_java_switch_enabled_on
                                                : R.string.bricks_java_switch_enabled_off)));

        // Interactive Enabled Compose Switch with Text
        ComposeMaterialSwitchWithText composeSwitchText =
                mView.findViewById(R.id.compose_switch_text);
        composeSwitchText.setChecked(true);
        composeSwitchText.setOnCheckedChangeListener(
                (buttonView, isChecked) ->
                        composeSwitchText.setText(
                                context.getString(
                                        isChecked
                                                ? R.string.bricks_java_switch_enabled_on
                                                : R.string.bricks_java_switch_enabled_off)));

        // Disabled On Compose Switch with Text
        ComposeMaterialSwitchWithText composeSwitchTextDisabledOn =
                mView.findViewById(R.id.compose_switch_text_disabled_on);
        composeSwitchTextDisabledOn.setChecked(true);
        composeSwitchTextDisabledOn.setEnabled(false);

        // Disabled Off Compose Switch with Text
        ComposeMaterialSwitchWithText composeSwitchTextDisabledOff =
                mView.findViewById(R.id.compose_switch_text_disabled_off);
        composeSwitchTextDisabledOff.setChecked(false);
        composeSwitchTextDisabledOff.setEnabled(false);

        // Interactive Enabled Compose Switch with Title & Summary
        ComposeMaterialSwitchWithTitleAndSummary composeSwitchTitle =
                mView.findViewById(R.id.compose_switch_title_summary);
        composeSwitchTitle.setTitleText(context.getString(R.string.bricks_java_switch_enabled_on));
        composeSwitchTitle.setSummaryText(
                context.getString(R.string.bricks_compose_switch_summary));
        composeSwitchTitle.setChecked(true);
        composeSwitchTitle.setOnCheckedChangeListener(
                (buttonView, isChecked) ->
                        composeSwitchTitle.setTitleText(
                                context.getString(
                                        isChecked
                                                ? R.string.bricks_java_switch_enabled_on
                                                : R.string.bricks_java_switch_enabled_off)));
    }

    @Override
    public View getView() {
        return mView;
    }

    @Override
    public void destroy() {}
}
