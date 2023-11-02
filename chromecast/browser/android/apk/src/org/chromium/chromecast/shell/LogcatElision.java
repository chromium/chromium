// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chromecast.shell;

import android.util.Patterns;

import java.util.regex.Matcher;
import java.util.regex.Pattern;

class LogcatElision {
    private static final String EMAIL_ELISION = "<EMAIL-ELIDED>";
    private static final String URL_ELISION = "<WEBADDRESS-ELIDED>";

    private static final String GOOD_IRI_CHAR = "a-zA-Z0-9\u00A0-\uD7FF\uF900-\uFDCF\uFDF0-\uFFEF";

    private static final Pattern IP_ADDRESS = Pattern.compile(
            "((25[0-5]|2[0-4][0-9]|[0-1][0-9]{2}|[1-9][0-9]|[1-9])\\.(25[0-5]|2[0-4]"
            + "[0-9]|[0-1][0-9]{2}|[1-9][0-9]|[1-9]|0)\\.(25[0-5]|2[0-4][0-9]|[0-1]"
            + "[0-9]{2}|[1-9][0-9]|[1-9]|0)\\.(25[0-5]|2[0-4][0-9]|[0-1][0-9]{2}"
            + "|[1-9][0-9]|[0-9]))");

    private static final String IRI =
            "[" + GOOD_IRI_CHAR + "]([" + GOOD_IRI_CHAR + "\\-]{0,61}[" + GOOD_IRI_CHAR + "]){0,1}";

    private static final String GOOD_GTLD_CHAR = "a-zA-Z\u00A0-\uD7FF\uF900-\uFDCF\uFDF0-\uFFEF";
    private static final String GTLD = "[" + GOOD_GTLD_CHAR + "]{2,63}";
    private static final String HOST_NAME = "(" + IRI + "\\.)+" + GTLD;

    private static final Pattern DOMAIN_NAME =
            Pattern.compile("(" + HOST_NAME + "|" + IP_ADDRESS + ")");

    private static final Pattern WEB_URL =
            Pattern.compile("(?:\\b|^)((?:(http|https|Http|Https|rtsp|Rtsp):"
                    + "\\/\\/(?:(?:[a-zA-Z0-9\\$\\-\\_\\.\\+\\!\\*\\'\\(\\)"
                    + "\\,\\;\\?\\&\\=]|(?:\\%[a-fA-F0-9]{2})){1,64}(?:\\:(?:[a-zA-Z0-9\\$\\-\\_"
                    + "\\.\\+\\!\\*\\'\\(\\)\\,\\;\\?\\&\\=]|(?:\\%[a-fA-F0-9]{2})){1,25})?\\@)?)?"
                    + "(?:" + DOMAIN_NAME + ")"
                    + "(?:\\:\\d{1,5})?)"
                    + "(\\/(?:(?:[" + GOOD_IRI_CHAR + "\\;\\/\\?\\:\\@\\&\\=\\#\\~"
                    + "\\-\\.\\+\\!\\*\\'\\(\\)\\,\\_])|(?:\\%[a-fA-F0-9]{2}))*)?"
                    + "(?:\\b|$)");

    private static final String IP_ELISION = "<IP-ELIDED>";
    private static final String MAC_ELISION = "<MAC-ELIDED>";

    private static final String CONSOLE_ELISION = "[ELIDED:CONSOLE(0)] ELIDED CONSOLE MESSAGE";

    private static final Pattern MAC_ADDRESS =
            Pattern.compile("([0-9a-fA-F]{2}[-:]+){5}[0-9a-fA-F]{2}");
    private static final Pattern CONSOLE_MSG = Pattern.compile("\\[\\w*:CONSOLE.*\\].*");

    private static final String[] CHROME_NAMESPACE = new String[] {"org.chromium.", "com.google."};

    private static final String[] CAST_NAMESPACE = new String[] {"Cast.", "CastV2."};

    private static final String[] SYSTEM_NAMESPACE = new String[] {"android.accessibilityservice",
            "android.accounts", "android.animation", "android.annotation", "android.app",
            "android.appwidget", "android.bluetooth", "android.content", "android.database",
            "android.databinding", "android.drm", "android.gesture", "android.graphics",
            "android.hardware", "android.inputmethodservice", "android.location", "android.media",
            "android.mtp", "android.net", "android.nfc", "android.opengl", "android.os",
            "android.preference", "android.print", "android.printservice", "android.provider",
            "android.renderscript", "android.sax", "android.security", "android.service",
            "android.speech", "android.support", "android.system", "android.telecom",
            "android.telephony", "android.test", "android.text", "android.transition",
            "android.util", "android.view", "android.webkit", "android.widget", "com.android.",
            "dalvik.", "java.", "javax.", "org.apache.", "org.json.", "org.w3c.dom.", "org.xml.",
            "org.xmlpull."};

    private static final Pattern JAVA_FILE = Pattern.compile(".java:[0-9]+$");

    private static final String[] LOG_SPAM = new String[] {"persist.mtk.mlog2logcat", "MLOG_KERN"};

    /**
     * Elides any emails in the specified {@link String} with {@link
     * #EMAIL_ELISION}.
     *
     * @param original String potentially containing emails.
     * @return String with elided emails.
     */
    private static String elideEmail(String original) {
        return Patterns.EMAIL_ADDRESS.matcher(original).replaceAll(EMAIL_ELISION);
    }
    /**
     * Elides any URLs in the specified {@link String} with
     * {@link #URL_ELISION}.
     *
     * @param original String potentially containing URLs.
     * @return String with elided URLs.
     */
    private static String elideUrl(String original) {
        StringBuilder buffer = new StringBuilder(original);
        Matcher matcher = WEB_URL.matcher(buffer);
        int start = 0;
        while (matcher.find(start)) {
            start = matcher.start();
            int end = matcher.end();
            String url = buffer.substring(start, end);
            if (!likelyToBeChromeNamespace(url) && !likelyToBeSystemNamespace(url)
                    && !likelyToBeCastNamespace(url) && !likelyToBeJavaFile(url)) {
                buffer.replace(start, end, URL_ELISION);
                end = start + URL_ELISION.length();
                matcher = WEB_URL.matcher(buffer);
            }
            start = end;
        }
        return buffer.toString();
    }

    private static boolean likelyToBeChromeNamespace(String url) {
        for (String ns : CHROME_NAMESPACE) {
            if (url.startsWith(ns)) {
                return true;
            }
        }
        return false;
    }

    private static boolean likelyToBeSystemNamespace(String url) {
        for (String ns : SYSTEM_NAMESPACE) {
            if (url.startsWith(ns)) {
                return true;
            }
        }
        return false;
    }

    private static boolean likelyToBeCastNamespace(String url) {
        for (String ns : CAST_NAMESPACE) {
            if (url.startsWith(ns)) {
                return true;
            }
        }
        return false;
    }

    private static boolean likelyToBeLogSpam(String logline) {
        for (String spam : LOG_SPAM) {
            if (logline.contains(spam)) {
                return true;
            }
        }
        return false;
    }

    public static boolean likelyToBeJavaFile(String url) {
        return JAVA_FILE.matcher(url).find();
    }
    /**
     * Elides any IP addresses in the specified {@link String} with {@link
     * #IP_ELISION}.
     *
     * @param original String potentially containing IPs.
     * @return String with elided IPs.
     */
    private static String elideIp(String original) {
        return Patterns.IP_ADDRESS.matcher(original).replaceAll(IP_ELISION);
    }
    /**
     * Elides any MAC addresses in the specified {@link String} with {@link
     * #MAC_ELISION}.
     *
     * @param original String potentially containing MACs.
     * @return String with elided MACs.
     */
    private static String elideMac(String original) {
        return MAC_ADDRESS.matcher(original).replaceAll(MAC_ELISION);
    }
    /**
     * Elides any console messages in the specified {@link String} with {@link
     * #CONSOLE_ELISION}.
     *
     * @param original String potentially containing console messages.
     * @return String with elided console messages.
     */
    private static String elideConsole(String original) {
        return CONSOLE_MSG.matcher(original).replaceAll(CONSOLE_ELISION);
    }

    public static String elide(String ln) {
        if (likelyToBeLogSpam(ln)) return "";
        ln = elideEmail(ln);
        ln = elideIp(ln);
        ln = elideUrl(ln);
        ln = elideMac(ln);
        ln = elideConsole(ln);
        return ln;
    }
}
