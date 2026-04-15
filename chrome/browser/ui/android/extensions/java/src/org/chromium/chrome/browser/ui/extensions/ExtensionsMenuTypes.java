// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.extensions;

import android.graphics.Bitmap;

import androidx.annotation.IntDef;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.JniType;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * Type definitions for the Extensions Menu. This class mirrors the C++ structures defined in {@link
 * ExtensionsMenuViewModel} to allow passing menu state across the JNI boundary to the Android UI.
 */
@JNINamespace("extensions")
@NullMarked
public class ExtensionsMenuTypes {
    /** Mirrors {@code ExtensionsMenuViewModel::ControlState} */
    public static class ControlState {
        @IntDef({Status.HIDDEN, Status.DISABLED, Status.ENABLED})
        @Retention(RetentionPolicy.SOURCE)
        public @interface Status {
            int HIDDEN = 0;
            int DISABLED = 1;
            int ENABLED = 2;
        }

        public final @Status int status;
        public final String text;
        public final String accessibleName;
        public final String tooltipText;
        public final boolean isOn;
        public final @Nullable Bitmap icon;

        @CalledByNative
        public ControlState(
                @Status int status,
                @JniType("std::u16string") String text,
                @JniType("std::u16string") String accessibleName,
                @JniType("std::u16string") String tooltipText,
                boolean isOn,
                @Nullable Bitmap icon) {
            this.status = status;
            this.text = text;
            this.accessibleName = accessibleName;
            this.tooltipText = tooltipText;
            this.isOn = isOn;
            this.icon = icon;
        }
    }

    /** Mirrors {@code ExtensionsMenuViewModel::MenuEntryState} */
    public static class MenuEntryState {
        public final String id;
        public final ControlState actionButton;
        public final ControlState contextMenuButton;
        public final ControlState siteAccessToggle;
        public final ControlState sitePermissionsButton;

        // TODO(crbug.com/471016915): add is enterprise boolean.

        @CalledByNative
        public MenuEntryState(
                @JniType("std::string") String id,
                ControlState actionButton,
                ControlState contextMenuButton,
                ControlState siteAccessToggle,
                ControlState sitePermissionsButton) {
            this.id = id;
            this.actionButton = actionButton;
            this.contextMenuButton = contextMenuButton;
            this.siteAccessToggle = siteAccessToggle;
            this.sitePermissionsButton = sitePermissionsButton;
        }
    }

    /** Mirrors {@code ExtensionsMenuViewModel::SiteSettingsState} */
    public static class SiteSettingsState {
        public final String label;
        public final boolean hasTooltip;
        public final ControlState toggle;

        @CalledByNative
        public SiteSettingsState(
                @JniType("std::u16string") String label, boolean hasTooltip, ControlState toggle) {
            this.label = label;
            this.hasTooltip = hasTooltip;
            this.toggle = toggle;
        }
    }

    /** Mirrors {@code ExtensionsMenuViewModel::OptionalSection} */
    @IntDef({
        OptionalSectionType.HOST_ACCESS_REQUESTS,
        OptionalSectionType.NONE,
        OptionalSectionType.RELOAD_PAGE
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface OptionalSectionType {
        int HOST_ACCESS_REQUESTS = 0;
        int NONE = 1;
        int RELOAD_PAGE = 2;
    }

    /** Mirrors {@code ExtensionsMenuViewModel::HostAccessRequest} */
    public static class HostAccessRequest {
        public final String extensionId;
        public final String extensionName;
        public final @Nullable Bitmap extensionIcon;

        @CalledByNative
        public HostAccessRequest(
                @JniType("std::string") String extensionId,
                @JniType("std::u16string") String extensionName,
                @Nullable Bitmap extensionIcon) {
            this.extensionId = extensionId;
            this.extensionName = extensionName;
            this.extensionIcon = extensionIcon;
        }
    }

    /** Mirrors {@code extensions::PermissionsManager::UserSiteAccess} */
    @IntDef({UserSiteAccess.ON_CLICK, UserSiteAccess.ON_SITE, UserSiteAccess.ON_ALL_SITES})
    @Retention(RetentionPolicy.SOURCE)
    public @interface UserSiteAccess {
        int ON_CLICK = 0;
        int ON_SITE = 1;
        int ON_ALL_SITES = 2;
    }

    /** Mirrors {@code ExtensionsMenuViewModel::ExtensionSitePermissionsState} */
    public static class ExtensionSitePermissionsState {
        public final String extensionName;
        public final @Nullable Bitmap extensionIcon;
        public final ControlState onClickOption;
        public final ControlState onSiteOption;
        public final ControlState onAllSitesOption;
        public final ControlState showRequestsToggle;

        @CalledByNative
        public ExtensionSitePermissionsState(
                @JniType("std::u16string") String extensionName,
                @Nullable Bitmap extensionIcon,
                ControlState onClickOption,
                ControlState onSiteOption,
                ControlState onAllSitesOption,
                ControlState showRequestsToggle) {
            this.extensionName = extensionName;
            this.extensionIcon = extensionIcon;
            this.onClickOption = onClickOption;
            this.onSiteOption = onSiteOption;
            this.onAllSitesOption = onAllSitesOption;
            this.showRequestsToggle = showRequestsToggle;
        }
    }
}
