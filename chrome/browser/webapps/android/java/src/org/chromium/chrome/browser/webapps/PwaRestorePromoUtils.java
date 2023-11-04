// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webapps;

import android.app.Activity;

import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetControllerProvider;
import org.chromium.components.webapps.pwa_restore_ui.PwaRestoreBottomSheetCoordinator;
import org.chromium.ui.base.WindowAndroid;

/**
 * This class is responsible for coordinating the showing of the PWA Restore promo (which aims to
 * remind users that they had PWAs installed on their old device, and can restore them on their new
 * device.
 */
public class PwaRestorePromoUtils {
    /**
     * Launch the PWA Restore promotion, if we've determined that this launch meets the criteria for
     * for showing it.
     *
     * @param activity The current {@link Activity} to use for this promo.
     * @param windowAndroid The current {@link WindowAndroid} to use for this promo.
     * @param arrowResourceId The resource id for the Back arrow to use.
     * @return Whether the PWA Restore promo was shown.
     */
    public static boolean launchPromoIfNeeded(
            Activity activity, WindowAndroid windowAndroid, int arrowResourceId) {
        if (!ChromeFeatureList.isEnabled(ChromeFeatureList.PWA_RESTORE_UI)) {
            return false;
        }

        // TODO(finnur): The criteria for this needs to be fleshed out, but the flag above is
        // disabled by default, so we can just attempt to show the dialog for now (which also helps
        // during development).
        return launchPromo(activity, windowAndroid, arrowResourceId);
    }

    private static boolean launchPromo(
            Activity activity, WindowAndroid windowAndroid, int arrowResourceId) {
        BottomSheetController controller = BottomSheetControllerProvider.from(windowAndroid);
        if (controller == null) return false;
        PwaRestoreBottomSheetCoordinator pwaRestoreBottomSheetCoordinator =
                new PwaRestoreBottomSheetCoordinator(activity, controller, arrowResourceId);
        return pwaRestoreBottomSheetCoordinator != null && pwaRestoreBottomSheetCoordinator.show();
    }
}
