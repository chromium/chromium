// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ANDROID_PLUS_ADDRESSES_ALL_PLUS_ADDRESSES_BOTTOM_SHEET_CONTROLLER_H_
#define CHROME_BROWSER_UI_ANDROID_PLUS_ADDRESSES_ALL_PLUS_ADDRESSES_BOTTOM_SHEET_CONTROLLER_H_

#include <memory>
#include <string>

#include "base/functional/callback.h"
#include "base/memory/raw_ref.h"
#include "base/types/optional_ref.h"
#include "ui/gfx/native_widget_types.h"

namespace content {
class WebContents;
}  // namespace content

class Profile;

namespace plus_addresses {

class AllPlusAddressesBottomSheetView;

// Android-only controller that opens the bottom sheet with all of the user's
// plus addresses. This bottom sheet is triggered from the keyboard accessory
// manual filling sheets.
class AllPlusAddressesBottomSheetController final {
 public:
  using SelectPlusAddressCallback =
      base::OnceCallback<void(base::optional_ref<const std::string>)>;

  explicit AllPlusAddressesBottomSheetController(
      content::WebContents* web_contents);

  AllPlusAddressesBottomSheetController(
      const AllPlusAddressesBottomSheetController&) = delete;
  AllPlusAddressesBottomSheetController& operator=(
      const AllPlusAddressesBottomSheetController&) = delete;
  AllPlusAddressesBottomSheetController(
      AllPlusAddressesBottomSheetController&&) = delete;
  AllPlusAddressesBottomSheetController& operator=(
      AllPlusAddressesBottomSheetController&&) = delete;

  ~AllPlusAddressesBottomSheetController();

  // Fetches the list of user's plus addresses and triggers the Java UI.
  // `on_plus_address_selected` is called when the bottom sheet is closed.
  void Show(SelectPlusAddressCallback on_plus_address_selected);

  void OnPlusAddressSelected(const std::string& plus_address);
  void OnBottomSheetDismissed();

  gfx::NativeView GetNativeView();
  Profile* GetProfile();

 private:
  const raw_ref<content::WebContents> web_contents_;
  std::unique_ptr<AllPlusAddressesBottomSheetView> view_;
  // This callback is called when the all plus addresses bottom sheet is closed.
  SelectPlusAddressCallback on_plus_address_selected_;
};

}  // namespace plus_addresses

#endif  // CHROME_BROWSER_UI_ANDROID_PLUS_ADDRESSES_ALL_PLUS_ADDRESSES_BOTTOM_SHEET_CONTROLLER_H_
