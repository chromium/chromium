// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.payments;

import org.chromium.base.Callback;
import org.chromium.url.GURL;

/** Checks the Content-Security-Policy (CSP). */
public interface CSPChecker {
    /**
     * Checks whether CSP connect-src directive allows the given URL. The parameters match
     * ContentSecurityPolicy::AllowConnectToSource() in:
     * third_party/blink/renderer/core/frame/csp/content_security_policy.h
     * @param url The URL to check.
     * @param urlBeforeRedirects The URL before redirects, if there was a redirect.
     * @param didFollowRedirect Whether there was a redirect.
     * @param resultCallback The callback to invoke with the result of the CSP check.
     */
    void allowConnectToSource(
            GURL url,
            GURL urlBeforeRedirects,
            boolean didFollowRedirect,
            Callback<Boolean> resultCallback);
}
