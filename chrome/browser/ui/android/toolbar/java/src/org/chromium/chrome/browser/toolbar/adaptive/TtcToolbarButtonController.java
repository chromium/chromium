// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.adaptive;

import android.content.Context;
import android.view.View;

import androidx.appcompat.content.res.AppCompatResources;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.toolbar.R;
import org.chromium.chrome.browser.toolbar.optional_button.BaseButtonDataProvider;
import org.chromium.chrome.browser.toolbar.optional_button.ButtonData.ButtonSpec;
import org.chromium.chrome.browser.ttc.TtcEntryPoint;

import java.util.function.Supplier;

/**
 * Optional toolbar button that starts or ends a TTC session, shown when TTC is chosen as the
 * toolbar shortcut. May be used by {@link AdaptiveToolbarButtonController}.
 */
@NullMarked
public class TtcToolbarButtonController extends BaseButtonDataProvider {
    private final TtcEntryPoint mEntryPoint;
    private final TtcEntryPoint.Observer mEntryPointObserver = this::onEntryPointStateChanged;

    /**
     * @param context The Context for retrieving resources.
     * @param activeTabSupplier Used to access the current tab.
     * @param entryPoint The window's TTC entry point, which decides whether TTC sessions can run
     *     and starts or ends them.
     */
    public TtcToolbarButtonController(
            Context context, Supplier<@Nullable Tab> activeTabSupplier, TtcEntryPoint entryPoint) {
        super(
                activeTabSupplier,
                /* modalDialogManager= */ null,
                new ButtonSpec.Builder(
                                AppCompatResources.getDrawable(
                                        context, R.drawable.ic_audio_spark_24dp),
                                context.getString(R.string.ttc_entrypoint_label),
                                /* supportsTinting= */ true)
                        .setButtonVariant(AdaptiveToolbarButtonVariant.TTC)
                        .setHoverTooltipTextId(R.string.ttc_entrypoint_label)
                        .build());
        mEntryPoint = entryPoint;
        mEntryPoint.addObserver(mEntryPointObserver);
    }

    @Override
    protected boolean shouldShowButton(@Nullable Tab tab) {
        return super.shouldShowButton(tab) && mEntryPoint.shouldShow();
    }

    @Override
    public void onClick(View view) {
        mEntryPoint.toggleSession();
    }

    @Override
    public void destroy() {
        mEntryPoint.removeObserver(mEntryPointObserver);
        super.destroy();
    }

    private void onEntryPointStateChanged() {
        notifyObservers(mEntryPoint.shouldShow());
    }
}
