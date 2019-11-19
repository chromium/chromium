// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.util;

/**
 * Java side version of chrome/common/url_constants.cc
 */
public class UrlConstants {
    public static final String CHROME_SCHEME = "chrome";
    public static final String CHROME_NATIVE_SCHEME = "chrome-native";
    public static final String CONTENT_SCHEME = "content";
    public static final String CUSTOM_TAB_SCHEME = "customtab";
    public static final String DATA_SCHEME = "data";
    public static final String DOCUMENT_SCHEME = "document";
    public static final String FILE_SCHEME = "file";
    public static final String FTP_SCHEME = "ftp";
    public static final String HTTP_SCHEME = "http";
    public static final String HTTPS_SCHEME = "https";
    public static final String INLINE_SCHEME = "inline";
    public static final String JAR_SCHEME = "jar";
    public static final String JAVASCRIPT_SCHEME = "javascript";
    public static final String SMS_SCHEME = "sms";
    public static final String TEL_SCHEME = "tel";

    public static final String CONTENT_URL_SHORT_PREFIX = "content:";
    public static final String CHROME_URL_SHORT_PREFIX = "chrome:";
    public static final String CHROME_NATIVE_URL_SHORT_PREFIX = "chrome-native:";
    public static final String FILE_URL_SHORT_PREFIX = "file:";
    public static final String DEVTOOLS_URL_SHORT_PREFIX = "devtools:";
    public static final String DEVTOOLS_FALLBACK_URL_SHORT_PREFIX = "chrome-devtools:";

    public static final String CHROME_URL_PREFIX = "chrome://";
    public static final String CHROME_NATIVE_URL_PREFIX = "chrome-native://";
    public static final String CONTENT_URL_PREFIX = "content://";
    public static final String FILE_URL_PREFIX = "file://";
    public static final String HTTP_URL_PREFIX = "http://";
    public static final String HTTPS_URL_PREFIX = "https://";

    public static final String ABOUT_URL = "chrome://about/";

    public static final String CHROME_BLANK_URL = "chrome://blank/";

    public static final String NTP_HOST = "newtab";
    public static final String NTP_URL = "chrome-native://newtab/";
    public static final String NTP_NON_NATIVE_URL = "chrome://newtab/";

    public static final String BOOKMARKS_HOST = "bookmarks";
    public static final String BOOKMARKS_URL = "chrome-native://bookmarks/";
    public static final String BOOKMARKS_FOLDER_URL = "chrome-native://bookmarks/folder/";
    public static final String BOOKMARKS_UNCATEGORIZED_URL =
            "chrome-native://bookmarks/uncategorized/";

    public static final String DOWNLOADS_HOST = "downloads";
    public static final String DOWNLOADS_URL = "chrome-native://downloads/";
    public static final String DOWNLOADS_FILTER_URL = "chrome-native://downloads/filter/";

    public static final String RECENT_TABS_HOST = "recent-tabs";
    public static final String RECENT_TABS_URL = "chrome-native://recent-tabs/";

    // TODO(dbeam): do we need both HISTORY_URL and NATIVE_HISTORY_URL?
    public static final String HISTORY_HOST = "history";
    public static final String HISTORY_URL = "chrome://history/";
    public static final String NATIVE_HISTORY_URL = "chrome-native://history/";

    public static final String INTERESTS_HOST = "interests";
    public static final String INTERESTS_URL = "chrome-native://interests/";

    public static final String GOOGLE_ACCOUNT_ACTIVITY_CONTROLS_URL =
            "https://myaccount.google.com/activitycontrols/search";

    public static final String MY_ACTIVITY_URL_IN_CBD =
            "https://myactivity.google.com/myactivity/?utm_source=chrome_cbd";

    public static final String MY_ACTIVITY_URL_IN_CBD_NOTICE =
            "https://myactivity.google.com/myactivity/?utm_source=chrome_n";

    public static final String MY_ACTIVITY_URL_IN_HISTORY =
            "https://myactivity.google.com/myactivity/?utm_source=chrome_h";

    public static final String EXPLORE_HOST = "explore";
    public static final String EXPLORE_URL = "chrome-native://explore/";
    public static final String CHROME_DINO_URL = "chrome://dino";
}
