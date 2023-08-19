// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.xsurface;

/**
 * Interface to supply chrome-specific colors to xsurface.
 *
 * Used to support dynamic themes on devices that support it.
 * The methods loosely mimics the Material Next dynamic color tokens.
 */
public interface ColorProvider {
    /** @return the primary color. */
    default int getPrimary() {
        return 0;
    }

    /** @return the on primary color. */
    default int getOnPrimary() {
        return 0;
    }

    /** @return the primary container color. */
    default int getPrimaryContainer() {
        return 0;
    }

    /** @return the primary on-container color. */
    default int getOnPrimaryContainer() {
        return 0;
    }

    /** @return the primary inverse color. */
    default int getPrimaryInverse() {
        return 0;
    }

    /** @return the secondary color. */
    default int getSecondary() {
        return 0;
    }

    /** @return the on secondary color. */
    default int getOnSecondary() {
        return 0;
    }

    /** @return the secondary container color. */
    default int getSecondaryContainer() {
        return 0;
    }

    /** @return the secondary on-container color. */
    default int getOnSecondaryContainer() {
        return 0;
    }

    /** @return the surface color. */
    default int getSurface() {
        return 0;
    }

    /** @return the on surface color. */
    default int getOnSurface() {
        return 0;
    }

    /** @return the surface variant color. */
    default int getSurfaceVariant() {
        return 0;
    }

    /** @return the on surface variant color. */
    default int getOnSurfaceVariant() {
        return 0;
    }

    /** @return the surface inverse color. */
    default int getSurfaceInverse() {
        return 0;
    }

    /** @return the on surface inverse color. */
    default int getOnSurfaceInverse() {
        return 0;
    }

    /** @return the error color. */
    default int getError() {
        return 0;
    }

    /** @return the on error color. */
    default int getOnError() {
        return 0;
    }

    /** @return the outline color. */
    default int getOutline() {
        return 0;
    }

    /** @return the divider color. */
    default int getDivider() {
        return 0;
    }
}
