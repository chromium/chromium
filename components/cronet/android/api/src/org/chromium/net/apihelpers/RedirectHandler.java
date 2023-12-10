// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net.apihelpers;

import org.chromium.net.UrlResponseInfo;

/** An interface for classes specifying how Cronet should behave on redirects. */
public interface RedirectHandler {
    /**
     * Returns whether the redirect should be followed.
     *
     * @param info the response info of the redirect response
     * @param newLocationUrl the redirect location
     * @return whether Cronet should follow teh redirect or not
     */
    boolean shouldFollowRedirect(UrlResponseInfo info, String newLocationUrl) throws Exception;
}
