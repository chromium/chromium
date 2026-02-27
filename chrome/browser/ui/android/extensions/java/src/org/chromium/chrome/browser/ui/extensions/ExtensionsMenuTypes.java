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

        @CalledByNative("ControlState")
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

        // TODO(crbug.com/471016915): add site permissions button.
        // TODO(crbug.com/471016915): add site access toggle.
        // TODO(crbug.com/471016915): add is enterprise boolean.

        @CalledByNative("MenuEntryState")
        public MenuEntryState(
                @JniType("std::string") String id,
                ControlState actionButton,
                ControlState contextMenuButton) {
            this.id = id;
            this.actionButton = actionButton;
            this.contextMenuButton = contextMenuButton;
        }
    }

    /** Mirrors {@code ExtensionsMenuViewModel::SiteSettingsState} */
    public static class SiteSettingsState {
        public final String label;
        public final boolean hasTooltip;
        public final ControlState toggle;

        @CalledByNative("SiteSettingsState")
        public SiteSettingsState(
                @JniType("std::u16string") String label, boolean hasTooltip, ControlState toggle) {
            this.label = label;
            this.hasTooltip = hasTooltip;
            this.toggle = toggle;
        }
    }
}
