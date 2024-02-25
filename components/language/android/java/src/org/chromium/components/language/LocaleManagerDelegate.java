// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.language;

import java.util.List;
import java.util.Locale;

/** Interface for {@link LocaleManager} APIs. */
public interface LocaleManagerDelegate {
    /**
     * Gets the currently set system App locale. Returns null if no override
     * locale is set.
     * @return List of system App locales.
     */
    public Locale getApplicationLocale();

    /**
     * Sets the system App locale.
     * @param Language tag to use as for the App UI at the system level.
     */
    public void setApplicationLocale(String languageName);

    /** @return The current system locales. */
    public List<Locale> getSystemLocales();
}
