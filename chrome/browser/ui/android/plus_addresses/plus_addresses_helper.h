// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ANDROID_PLUS_ADDRESSES_PLUS_ADDRESSES_HELPER_H_
#define CHROME_BROWSER_UI_ANDROID_PLUS_ADDRESSES_PLUS_ADDRESSES_HELPER_H_

namespace content {
class WebContents;
}

namespace plus_addresses {

// Opens a manage plus addresses web page in a Chrome Custom Tab.
void ShowManagePlusAddressesPage(content::WebContents& web_contents);

}  // namespace plus_addresses

#endif  // CHROME_BROWSER_UI_ANDROID_PLUS_ADDRESSES_PLUS_ADDRESSES_HELPER_H_
