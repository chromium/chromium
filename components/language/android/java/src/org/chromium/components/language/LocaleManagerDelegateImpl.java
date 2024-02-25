// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.language;

import java.util.ArrayList;
import java.util.List;
import java.util.Locale;

/** Interface for {@link LocaleManager} APIs. */
public class LocaleManagerDelegateImpl implements LocaleManagerDelegate {
    /**
     * Gets the currently set system App locale. Returns null if no override
     * locale is set.
     * @return List of system App locales.
     */
    @Override
    public Locale getApplicationLocale() {
        // TODO(https://crbug.com/1348676): Replace with calls to {@link LocaleManager} once the T
        // SDK is available.
        return null;
    }

    /**
     * Sets the system App locale. If |languageName| is {@link APP_LOCALE_USE_SYSTEM_LANGUAGE} or
     * null then the App locale will be set to follow the system default.
     * @param Language tag to use as for the App UI at the system level.
     */
    @Override
    public void setApplicationLocale(String languageName) {
        // TODO(https://crbug.com/1348676): Replace with calls to {@link LocaleManager} once the T
        // SDK is available.
        return;
    }

    /** The default implementation returns a list with the current Java locale. */
    @Override
    public List<Locale> getSystemLocales() {
        ArrayList<Locale> locales = new ArrayList<Locale>();
        locales.add(Locale.getDefault());
        return locales;
    }
}
