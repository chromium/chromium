// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.embedder_support.util;

import org.chromium.url.GURL;

/**
 * Java side version of chrome/common/url_constants.cc
 *
 * <p>Do not add any more NTP related constants. TODO(crbug.com/40281619) Move NTP related constants
 * to ChromeUrlConstants.java
 */
public class UrlConstants {
    public static final String APP_INTENT_SCHEME = "android-app";
    public static final String BLOB_SCHEME = "blob";
    public static final String CHROME_SCHEME = "chrome";
    public static final String CHROME_NATIVE_SCHEME = "chrome-native";
    public static final String CONTENT_SCHEME = "content";
    public static final String CUSTOM_TAB_SCHEME = "customtab";
    public static final String DATA_SCHEME = "data";
    public static final String DEVTOOLS_SCHEME = "devtools";
    public static final String DOCUMENT_SCHEME = "document";
    public static final String FIDO_SCHEME = "fido";
    public static final String FILE_SCHEME = "file";
    public static final String FILESYSTEM_SCHEME = "filesystem";
    public static final String FTP_SCHEME = "ftp";
    public static final String HTTP_SCHEME = "http";
    public static final String HTTPS_SCHEME = "https";
    public static final String INLINE_SCHEME = "inline";
    public static final String INTENT_SCHEME = "intent";
    public static final String JAR_SCHEME = "jar";
    public static final String JAVASCRIPT_SCHEME = "javascript";
    public static final String SMS_SCHEME = "sms";
    public static final String TEL_SCHEME = "tel";

    public static final String CONTENT_URL_SHORT_PREFIX = "content:";
    public static final String CHROME_URL_SHORT_PREFIX = "chrome:";
    public static final String CHROME_NATIVE_URL_SHORT_PREFIX = "chrome-native:";
    public static final String FILE_URL_SHORT_PREFIX = "file:";

    public static final String CHROME_URL_PREFIX = "chrome://";
    public static final String CHROME_NATIVE_URL_PREFIX = "chrome-native://";
    public static final String CONTENT_URL_PREFIX = "content://";
    public static final String FILE_URL_PREFIX = "file://";
    public static final String HTTP_URL_PREFIX = "http://";
    public static final String HTTPS_URL_PREFIX = "https://";

    public static final String ABOUT_URL = "chrome://about/";

    public static final String NTP_HOST = "newtab";
    public static final String NTP_URL = "chrome-native://newtab/";
    public static final String NTP_NON_NATIVE_URL = "chrome://newtab/";
    public static final String NTP_ABOUT_URL = "about:newtab";

    // Don't use this URL. The constant is added for legacy reasons.
    public static final String NEW_TAB_PAGE_URL_LEGACY = "chrome://new-tab-page/";

    public static final String BOOKMARKS_HOST = "bookmarks";
    public static final String BOOKMARKS_URL = "chrome://bookmarks/";
    public static final String BOOKMARKS_NATIVE_URL = "chrome-native://bookmarks/";
    public static final String BOOKMARKS_FOLDER_URL = "chrome-native://bookmarks/folder/";
    public static final String BOOKMARKS_UNCATEGORIZED_URL =
            "chrome-native://bookmarks/uncategorized/";

    public static final String DOWNLOADS_HOST = "downloads";
    public static final String DOWNLOADS_URL = "chrome-native://downloads/";
    public static final String DOWNLOADS_FILTER_URL = "chrome-native://downloads/filter/";

    public static final String RECENT_TABS_HOST = "recent-tabs";
    public static final String RECENT_TABS_URL = "chrome-native://recent-tabs/";
    public static final String GRID_TAB_SWITCHER_URL = "chrome-native://gts/";

    // TODO(dbeam): do we need both HISTORY_URL and NATIVE_HISTORY_URL?
    public static final String HISTORY_HOST = "history";
    public static final String HISTORY_URL = "chrome://history/";
    public static final String NATIVE_HISTORY_URL = "chrome-native://history/";

    public static final String LAUNCHPAD_HOST = "apps";
    public static final String LAUNCHPAD_URL = "chrome://apps/";

    public static final String INTERESTS_HOST = "interests";
    public static final String INTERESTS_URL = "chrome-native://interests/";

    public static final String GPU_URL = "chrome://gpu/";
    public static final String VERSION_URL = "chrome://version/";

    public static final String GOOGLE_ACCOUNT_HOME_URL = "https://myaccount.google.com/";

    public static final String GOOGLE_ACCOUNT_ACTIVITY_CONTROLS_URL =
            "https://myaccount.google.com/activitycontrols/search";

    public static final String GOOGLE_ACCOUNT_LINKED_SERVICES_URL =
            "https://myaccount.google.com/linked-services?utm_source=chrome_s";

    public static final String GOOGLE_ACCOUNT_ACTIVITY_CONTROLS_FROM_PG_URL =
            "https://myaccount.google.com/activitycontrols/search"
                    + "&utm_source=chrome&utm_medium=privacy-guide";

    public static final String GOOGLE_ACCOUNT_DEVICE_ACTIVITY_URL =
            "https://myaccount.google.com/device-activity?utm_source=chrome";

    public static final String MY_ACTIVITY_HOME_URL = "https://myactivity.google.com/";

    public static final String GOOGLE_SEARCH_HISTORY_URL_IN_CBD =
            "https://myactivity.google.com/product/search?utm_source=chrome_cbd";

    public static final String MY_ACTIVITY_URL_IN_CBD =
            "https://myactivity.google.com/myactivity?utm_source=chrome_cbd";

    public static final String MY_ACTIVITY_URL_IN_CBD_NOTICE =
            "https://myactivity.google.com/myactivity/?utm_source=chrome_n";

    public static final String MY_ACTIVITY_URL_IN_HISTORY =
            "https://myactivity.google.com/myactivity/?utm_source=chrome_h";

    public static final String GOOGLE_SEARCH_HISTORY_URL_IN_QD =
            "https://myactivity.google.com/product/search?utm_source=chrome_qd";

    public static final String MY_ACTIVITY_URL_IN_QD =
            "https://myactivity.google.com/myactivity?utm_source=chrome_qd";

    public static final String GOOGLE_EMBEDDED_PRIVACY_POLICY =
            "https://policies.google.com/privacy/embedded";

    public static final String GOOGLE_EMBEDDED_PRIVACY_POLICY_DARK_MODE =
            "https://policies.google.com/privacy/embedded?color_scheme=dark";

    public static final String GOOGLE_URL = "https://www.google.com/";

    public static final String EXPLORE_HOST = "explore";
    public static final String EXPLORE_URL = "chrome-native://explore/";
    public static final String CHROME_DINO_URL = "chrome://dino/";

    public static final String LOCALHOST = "localhost";

    public static final String MANAGEMENT_HOST = "management";
    public static final String MANAGEMENT_URL = "chrome://management/";

    /* Host and url used for PDF native pages. */
    public static final String PDF_HOST = "pdf";
    public static final String PDF_URL = "chrome-native://pdf/";
    public static final String PDF_URL_PARAM = "link?url=";
    public static final String PDF_URL_QUERY_PARAM = "url";

    private static class Holder {
        private static final String SERIALIZED_NTP_URL =
                "73,1,true,0,6,0,-1,0,-1,9,6,0,-1,15,1,0,-1,0,-1,false,false,chrome://newtab/";
        private static GURL sNtpGurl =
                GURL.deserializeLatestVersionOnly(SERIALIZED_NTP_URL.replace(',', '\0'));
    }

    /**
     * Returns a cached GURL representation of {@link UrlConstants.NTP_NON_NATIVE_URL}. It is safe
     * to call this method before native is loaded and doing so will not block on native loading
     * completion since a hardcoded, serialized string is used.
     */
    public static GURL ntpGurl() {
        return Holder.sNtpGurl;
    }
}
