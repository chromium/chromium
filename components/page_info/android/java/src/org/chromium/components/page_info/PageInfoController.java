// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.page_info;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.components.content_settings.ContentSettingValues;
import org.chromium.content_public.browser.WebContents;

/**
 * Java side of Android implementation of the page info UI.
 */
public class PageInfoController {

    /**
     * Adds a new row for the given permission.
     *
     * @param name The title of the permission to display to the user.
     * @param nameMidSentence The title of the permission to display to the user when used
     *         mid-sentence.
     * @param type The ContentSettingsType of the permission.
     * @param currentSettingValue The ContentSetting value of the currently selected setting.
     */
    @CalledByNative
    private void addPermissionSection(String name, String nameMidSentence, int type,
            @ContentSettingValues int currentSettingValue) {
    }

    /**
     * Update the permissions view based on the contents of mDisplayedPermissions.
     */
    @CalledByNative
    private void updatePermissionDisplay() {
    }

    /**
     * Sets the connection security summary and detailed description strings. These strings may be
     * overridden based on the state of the Android UI.
     */
    @CalledByNative
    private void setSecurityDescription(String summary, String details) {

    }

    /**
     * Updates the Topic view if present.
     */
    @CalledByNative
    private void updateTopicsDisplay(String[] topics) {

    }

    @NativeMethods
    interface Natives {
        long init(PageInfoController controller, WebContents webContents);
        void destroy(long nativePageInfoControllerAndroid, PageInfoController caller);
        void recordPageInfoAction(
                long nativePageInfoControllerAndroid, PageInfoController caller, int action);
        void setAboutThisSiteShown(long nativePageInfoControllerAndroid, PageInfoController caller,
                boolean wasAboutThisSiteShown);
        void updatePermissions(long nativePageInfoControllerAndroid, PageInfoController caller);
    }
}
