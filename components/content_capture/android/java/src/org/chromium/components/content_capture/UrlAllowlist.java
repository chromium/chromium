// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.content_capture;

import android.net.Uri;

import java.util.HashSet;
import java.util.List;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

/**
 * This is helper classes for ContentCaptureConsumer to implement shouldCapture method. Only host
 * is matched.
 */
public class UrlAllowlist {
    private HashSet<String> mAllowedUrls;
    private List<Pattern> mAllowedRe;

    /** Nothing is allowed if both allowedUrls and allowedRes is null or empty. */
    public UrlAllowlist(HashSet<String> allowedUrls, List<Pattern> allowedRe) {
        mAllowedUrls = allowedUrls;
        mAllowedRe = allowedRe;
    }

    /** @return if any of the given urls's host allowed. */
    public boolean isAllowed(String[] urls) {
        if (mAllowedRe == null && mAllowedUrls == null) return false;
        for (String url : urls) {
            Uri uri = Uri.parse(url);
            String host = uri.getHost();
            if (host != null) {
                if (mAllowedUrls != null && mAllowedUrls.contains(host)) return true;
                if (mAllowedRe != null) {
                    for (Pattern p : mAllowedRe) {
                        Matcher m = p.matcher(host);
                        if (m.find()) return true;
                    }
                }
            }
        }
        return false;
    }
}
