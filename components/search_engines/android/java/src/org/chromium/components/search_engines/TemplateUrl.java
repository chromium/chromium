// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.components.search_engines;

import org.jni_zero.CalledByNative;
import org.jni_zero.NativeMethods;

import org.chromium.build.annotations.MockedInTests;

import java.util.Locale;

/**
 * Represents object of a search engine. It only caches the native pointer of TemplateURL object
 * from native side. Any class uses this need to register a {@link TemplateUrlServiceObserver} on
 * {@link TemplatUrlService} to listen the native changes in case the native pointer is destroyed.
 */
@MockedInTests
public class TemplateUrl {
    private final long mTemplateUrlPtr;

    @CalledByNative
    private static TemplateUrl create(long templateUrlPtr) {
        return new TemplateUrl(templateUrlPtr);
    }

    protected TemplateUrl(long templateUrlPtr) {
        mTemplateUrlPtr = templateUrlPtr;
    }

    /** @return The name of the search engine. */
    public String getShortName() {
        return TemplateUrlJni.get().getShortName(mTemplateUrlPtr);
    }

    /**
     * @return The prepopulated id of the search engine. For predefined engines, this field is a
     *     non-zero, for custom search engines, it will return 0.
     */
    public int getPrepopulatedId() {
        return TemplateUrlJni.get().getPrepopulatedId(mTemplateUrlPtr);
    }

    /** @return Whether a search engine is prepopulated or created by policy. */
    public boolean getIsPrepopulated() {
        return TemplateUrlJni.get().isPrepopulatedOrDefaultProviderByPolicy(mTemplateUrlPtr);
    }

    /** @return The keyword of the search engine. */
    public String getKeyword() {
        return TemplateUrlJni.get().getKeyword(mTemplateUrlPtr);
    }

    /**
     * @return The last time used this search engine. If a search engine hasn't been used, it will
     *     return 0.
     */
    public long getLastVisitedTime() {
        return TemplateUrlJni.get().getLastVisitedTime(mTemplateUrlPtr);
    }

    /**
     * @return The template URL of the search engine. The format can be looked up in
     *     prepopulated_engines.json.
     */
    public String getURL() {
        return TemplateUrlJni.get().getURL(mTemplateUrlPtr);
    }

    /**
     * @return The new Tab URL of the search engine. The format can be looked up in
     *     prepopulated_engines.json.
     */
    public String getNewTabURL() {
        return TemplateUrlJni.get().getNewTabURL(mTemplateUrlPtr);
    }

    public long getNativePtr() {
        return mTemplateUrlPtr;
    }

    @Override
    public boolean equals(Object other) {
        if (!(other instanceof TemplateUrl)) return false;
        TemplateUrl otherTemplateUrl = (TemplateUrl) other;
        return mTemplateUrlPtr == otherTemplateUrl.mTemplateUrlPtr;
    }

    @Override
    public String toString() {
        return String.format(
                Locale.US,
                "TemplateURL -- keyword: %s, short name: %s, " + "prepopulated: %b",
                getKeyword(),
                getShortName(),
                getIsPrepopulated());
    }

    @NativeMethods
    public interface Natives {
        String getShortName(long templateUrlPtr);

        String getKeyword(long templateUrlPtr);

        boolean isPrepopulatedOrDefaultProviderByPolicy(long templateUrlPtr);

        long getLastVisitedTime(long templateUrlPtr);

        int getPrepopulatedId(long templateUrlPtr);

        String getURL(long templateUrlPtr);

        String getNewTabURL(long templateUrlPtr);
    }
}
