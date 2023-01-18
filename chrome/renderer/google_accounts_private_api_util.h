// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_GOOGLE_ACCOUNTS_PRIVATE_API_UTIL_H_
#define CHROME_RENDERER_GOOGLE_ACCOUNTS_PRIVATE_API_UTIL_H_

namespace content {
class RenderFrame;
}  // namespace content

// Checks that the input frame has a Gaia Origin.
// The logic in this function should be consistent with the logic in
// `ShouldExposeGoogleAccountsPrivateApi()` in
// chrome/browser/signin/google_accounts_private_api_util.h, because the
// Javascript API simply exposes the Google Accounts Private API to the web
// page, and hence the Javascript API shouldn't be available if the API isn't.
bool ShouldExposeGoogleAccountsJavascriptApi(
    content::RenderFrame* render_frame);

#endif  // CHROME_RENDERER_GOOGLE_ACCOUNTS_PRIVATE_API_UTIL_H_
