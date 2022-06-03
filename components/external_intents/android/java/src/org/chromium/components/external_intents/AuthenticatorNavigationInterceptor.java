// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.external_intents;

/**
 * Handles intercepting navigation requests for an external authenticator application.
 */
public interface AuthenticatorNavigationInterceptor {
    /**
     * To be called by a Tab to check whether the passed in URL, which is about to be loaded,
     * should be processed by an external Authenticator application.
     *
     * @param url the URL about to be loaded in the tab
     * @return True if the URL has been handled by the Authenticator, false if it hasn't and
     *         should be processed normally by the Tab.
     */
    boolean handleAuthenticatorUrl(String url);
}
